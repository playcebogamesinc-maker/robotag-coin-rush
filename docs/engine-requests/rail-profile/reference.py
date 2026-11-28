#!/usr/bin/env python3
"""Reference for engine request #11 (../rail-profile.md): a grooved rail profile with junction
cut-outs, built from a .rails.json. It only makes a preview .glb for screenshots; the real
thing belongs in the engine's rails module (modules/rails/render).

Usage (from the engine root; needs `pip install shapely`):
  python3 games/coin-rush/docs/engine-requests/rail-profile/reference.py \
      games/coin-rush/content/scenes/main.rails.json OUT/rails_preview.glb [contact_y]

The method, in plan view (x, z) of a flat railway:
  1. Centre lines: every rail, plus every junction rail (the engine's corner curves: a cubic
     Bezier from the switch point on one arm to the switch point on the other, tangent to both,
     handles chord / 3; switch point = junction_radius * tan(turn / 2) from the node).
  2. The profile is bands from the centre out, each a half width and a height (relative to the
     contact line = where the wheel's bottom rides). Band 0 is the groove.
  3. U_k = union over all rails of "centre line grown by band k's half width". The top surface
     at a point is the height of the innermost band that covers it:
       region_0 = U_0, region_k = U_k - U_(k-1).
     So every rail's groove cuts through every other rail's lips: at a T, an X, a corner and
     along each corner curve's switch the wheel groove stays open.
  4. Each region is triangulated flat at its height; walls go down along every region edge
     to whatever is next to it (a lower band, or the ground)."""
import json, math, struct, sys

from shapely import constrained_delaunay_triangles
from shapely.geometry import LineString, Point, Polygon, MultiPolygon
from shapely.ops import unary_union
from shapely.prepared import prep

# The profile: (name, half width m, height above the contact line m, material slot).
BANDS = [
    ("groove", 0.13, 0.003, "groove"),   # The wheel (0.2 m wide) runs in here.
    ("lip",    0.20, 0.06,  "rail"),     # The two raised edges.
    ("foot",   0.26, -0.06, "rail"),     # A low flange on the floor.
]
GROUND = 0.003         # Floor top (the tiles).
JUNCTION_RADIUS = 4.0  # The engine's default (RailNode.junction_radius).
CURVE_STEP = 0.15      # Sampling of corner curves, m.


def port_dir(angle):
    a = math.radians(angle)
    return (-math.sin(a), -math.cos(a))        # Unrotated node: 0 = -z, 90 = -x.


def bezier(p0, p1, p2, p3, steps):
    out = []
    for i in range(steps + 1):
        u = i / steps
        v = 1 - u
        out.append(tuple(v * v * v * a + 3 * v * v * u * b + 3 * v * u * u * c + u * u * u * d
                         for a, b, c, d in zip(p0, p1, p2, p3)))
    return out


def centre_lines(rails):
    nodes = {n["id"]: n for n in rails["nodes"]}
    pos = lambda n: (n["position"][0], n["position"][2])
    lines, arms = [], {}
    for e in rails["edges"]:
        a, b = nodes[e["from"][0]], nodes[e["to"][0]]
        assert not e.get("knots"), "the reference handles straight rails only"
        lines.append(("straight", [pos(a), pos(b)]))
        for end in ("from", "to"):
            nid, port = e[end]
            other = b if end == "from" else a
            arms[(nid, port)] = math.dist(pos(nodes[nid]), pos(other))
    for n in rails["nodes"]:
        ports = n.get("ports", [0, 180])
        if len(ports) < 3:
            continue                              # Plain node or corner: no junction rails.
        for i in range(len(ports)):
            for j in range(i + 1, len(ports)):
                da, db = port_dir(ports[i]), port_dir(ports[j])
                c = max(-1.0, min(1.0, da[0] * db[0] + da[1] * db[1]))
                if c <= math.cos(math.radians(160)):
                    continue                      # Straight across (>= 160 deg): the arms.
                turn = math.pi - math.acos(c)
                d = n.get("junction_radius", JUNCTION_RADIUS) * math.tan(turn / 2)
                d = min(d, 0.4 * arms[(n["id"], i)], 0.4 * arms[(n["id"], j)])
                o = pos(n)
                A = (o[0] + da[0] * d, o[1] + da[1] * d)
                B = (o[0] + db[0] * d, o[1] + db[1] * d)
                chord = math.dist(A, B)
                h = chord / 3
                pts = bezier(A, (A[0] - da[0] * h, A[1] - da[1] * h),
                             (B[0] - db[0] * h, B[1] - db[1] * h), B,
                             max(8, int(chord / CURVE_STEP)))
                lines.append(("junction", pts))
    return lines


def build(rails, contact_y):
    lines = centre_lines(rails)
    unions = []
    for _, hw, _, _ in BANDS:
        parts = []
        for kind, pts in lines:
            # Straight rails end square past their nodes (corners and T stems close up);
            # corner curves end flat (they merge into the arms they are tangent to).
            cap = "square" if kind == "straight" else "flat"
            parts.append(LineString(pts).buffer(hw, cap_style=cap, join_style="mitre",
                                                quad_segs=4))
        unions.append(unary_union(parts).simplify(0.002))
    regions = [unions[0]] + [unions[k].difference(unions[k - 1]) for k in range(1, len(BANDS))]
    # No needle-thin tips where two grooves run apart at a switch (shrink 5 mm, grow back).
    # A band's slivers join the band inside it (lip -> groove, foot -> lip).
    for k in range(len(regions) - 1, 0, -1):
        opened = regions[k].buffer(-0.005, join_style="mitre").buffer(0.005, join_style="mitre")
        regions[k - 1] = regions[k - 1].union(regions[k].difference(opened))
        regions[k] = regions[k].intersection(opened)
    unions = [unary_union(regions[:k + 1]) for k in range(len(regions))]
    prepared = [prep(u) for u in unions]
    heights = [contact_y + b[2] for b in BANDS]

    def height_at(x, z):
        p = Point(x, z)
        for k, u in enumerate(prepared):
            if u.contains(p):
                return heights[k]
        return GROUND

    parts = {}                                    # slot -> (positions, normals, indices)

    def tri(slot, a, b, c, n):
        pos, nor, idx = parts.setdefault(slot, ([], [], []))
        base = len(pos)
        pos.extend([a, b, c])
        nor.extend([n] * 3)
        idx.extend([base, base + 1, base + 2])

    eps = 0.002
    for k, region in enumerate(regions):
        y = heights[k]
        slot = BANDS[k][3]
        polys = list(region.geoms) if isinstance(region, MultiPolygon) else [region]
        for poly in polys:
            if poly.is_empty or poly.area < 1e-6:
                continue
            # Top: flat triangles facing up.
            for t in constrained_delaunay_triangles(poly).geoms:
                (ax, az), (bx, bz), (cx, cz) = list(t.exterior.coords)[:3]
                ny = (bz - az) * (cx - ax) - (bx - ax) * (cz - az)
                a, b, c = (ax, y, az), (bx, y, bz), (cx, y, cz)
                tri(slot, a, b, c, (0, 1, 0)) if ny > 0 else tri(slot, a, c, b, (0, 1, 0))
            # Walls down to the neighbour on every edge.
            region_p = prep(poly)
            for ring in [poly.exterior, *poly.interiors]:
                c = list(ring.coords)
                for (px, pz), (qx, qz) in zip(c, c[1:]):
                    L = math.hypot(qx - px, qz - pz)
                    if L < 1e-5:
                        continue
                    nx, nz = (qz - pz) / L, -(qx - px) / L
                    mx, mz = (px + qx) / 2, (pz + qz) / 2
                    if region_p.contains(Point(mx + nx * eps, mz + nz * eps)):
                        nx, nz = -nx, -nz         # Point the normal out of the region.
                    lower = height_at(mx + nx * eps, mz + nz * eps)
                    if lower >= y - 1e-4:
                        continue
                    a, b = (px, y, pz), (qx, y, qz)
                    a0, b0 = (px, lower, pz), (qx, lower, qz)
                    # Counter-clockwise seen from outside (normal (nx, 0, nz)).
                    ux, uz = qx - px, qz - pz
                    if ux * -nz + uz * nx > 0:    # a -> b runs to the right seen from outside.
                        a, b, a0, b0 = b, a, b0, a0
                    tri(slot, a0, b0, b, (nx, 0, nz))
                    tri(slot, a0, b, a, (nx, 0, nz))
    return parts


def write_glb(path, parts, colors):
    blob, views, accessors, prims, mats = bytearray(), [], [], [], []

    def view(data, target):
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data),
                      "target": target})
        blob.extend(data)
        return len(views) - 1

    for i, (name, (pos, nor, idx)) in enumerate(sorted(parts.items())):
        pv = view(b"".join(struct.pack("<3f", *p) for p in pos), 34962)
        nv = view(b"".join(struct.pack("<3f", *n) for n in nor), 34962)
        iv = view(struct.pack(f"<{len(idx)}I", *idx), 34963)
        accessors += [
            {"bufferView": pv, "componentType": 5126, "count": len(pos), "type": "VEC3",
             "min": [min(p[k] for p in pos) for k in range(3)],
             "max": [max(p[k] for p in pos) for k in range(3)]},
            {"bufferView": nv, "componentType": 5126, "count": len(nor), "type": "VEC3"},
            {"bufferView": iv, "componentType": 5125, "count": len(idx), "type": "SCALAR"}]
        a = len(accessors) - 3
        prims.append({"attributes": {"POSITION": a, "NORMAL": a + 1}, "indices": a + 2,
                      "material": i})
        mats.append({"name": name, "pbrMetallicRoughness": {
            "baseColorFactor": colors[name] + [1.0], "metallicFactor": 0.0,
            "roughnessFactor": 0.5}})
    gltf = {"asset": {"version": "2.0", "generator": "reference.py"},
            "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": "rails"}],
            "meshes": [{"primitives": prims}], "materials": mats,
            "buffers": [{"byteLength": len(blob)}], "bufferViews": views,
            "accessors": accessors}
    js = json.dumps(gltf, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    while len(blob) % 4:
        blob.append(0)
    out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob))
    out += struct.pack("<II", len(js), 0x4E4F534A) + js
    out += struct.pack("<II", len(blob), 0x004E4942) + bytes(blob)
    open(path, "wb").write(out)


if __name__ == "__main__":
    rails = json.load(open(sys.argv[1]))
    contact = float(sys.argv[3]) if len(sys.argv) > 3 else 0.12
    parts = build(rails, contact)
    write_glb(sys.argv[2], parts, {"groove": [0.1, 0.08, 0.07], "rail": [0.2, 0.17, 0.15]})
    print({k: len(v[2]) // 3 for k, v in parts.items()}, "triangles")
