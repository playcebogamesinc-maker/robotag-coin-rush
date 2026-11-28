#!/usr/bin/env python3
"""Coin Rush's level look: Egyptian pyramid tunnels built from basic shapes (docs/environment.md).

Reads the maze (the "Wall NN" boxes in the scene and the rails) and writes:
  - content/models/env/{architecture,floor,props,flames}.glb: every pillar, beam, tile, prop
    and torch flame merged into four meshes (four draws on the phone instead of thousands);
  - OUT/scene.json: rt commands that raise the walls into a tunnel, add the ceiling, the four
    environment entities, the light and the look (ids e_e9a7...; a rerun replaces them).
Only static scenery: rails, coins, robots and UI are never touched.

Usage (from the engine root):
  python3 games/coin-rush/tools/environment.py games/coin-rush OUT
  build/linux/bin/Debug/rt apply games/coin-rush/content/scenes/main.scene.json OUT/scene.json

Corridors: every rail is a corridor 4 m wide (walls 2 m from the rail). Its station points
every ~3 m carry a pillar pair and a ceiling beam; the bays between them get props by the
corridor's theme (THEMES), so neighbouring corridors look different. Nothing comes closer
than 1.05 m to a rail (torch cups), the junction squares stay empty."""
import json, math, os, random, struct, sys

WALL_H = 5.6         # Walls reach the ceiling.
CEIL_Y = 5.6
WALL_SKIN = 0.02     # The lit wall faces sit this far in front of the "Wall NN" boxes.
HALF = 2.0           # Rail centre to wall face.
STATION = 3.0        # Pillar spacing along a corridor.
END = 3.0            # First / last pillar this far from a junction node.
TILE = 1.0           # Floor tile size (rails sit on tile seams).
SEED = 7
AMBIENT = 0.1        # Cool dim fill everywhere (the scene has no sun); the torches are warm.
AMBIENT_COLOR = [0.5, 0.6, 0.85]

# Corridor themes, assigned round-robin over a shuffled corridor list (SEED).
THEMES = ["hall", "gallery", "storage", "glyphs", "steps", "hall", "storage", "glyphs"]

# Material slot -> material file (content/materials/env/); the files are made by the tool too.
# Stone is textured (tools/textures.py, 2 m per image: tiling 0.5): triplanar on walls and
# props, projected from above on the floor. base_color tints the image.
SAND = {"base_map": "textures/env/sandstone.png", "normal_map": "textures/env/sandstone_n.png",
        "mapping": "triplanar", "tiling": [0.5, 0.5], "roughness": 0.9, "rim": False,
        "shadow_tint": [0.42, 0.3, 0.3], "band_softness": 0.25}
OUTLINE = {"outline": 1.2, "outline_color": [0.03, 0.017, 0.01]}
MATERIALS = {
    "wall":    {**SAND, "base_color": [0.82, 0.74, 0.66]},
    "stone":   {**SAND, "base_color": [0.9, 0.8, 0.7], "tiling": [0.7, 0.7]},
    "stone_b": {**SAND, "base_color": [0.74, 0.64, 0.56], "tiling": [0.7, 0.7]},
    "trim":    {**SAND, "base_color": [1.0, 0.9, 0.78], "tiling": [1.0, 1.0]},
    "block":   {**SAND, **OUTLINE, "base_color": [0.86, 0.76, 0.66], "tiling": [0.8, 0.8]},
    "ceiling": {**SAND, "base_color": [0.4, 0.34, 0.3]},
    "tile":    {**SAND, "base_map": "textures/env/slabs.png",
                "normal_map": "textures/env/slabs_n.png", "mapping": "world",
                "base_color": [0.85, 0.76, 0.68]},
    "glyph":   {**SAND, "base_map": "textures/env/glyphs.png",
                "normal_map": "textures/env/glyphs_n.png", "base_color": [0.9, 0.78, 0.66]},
    "statue":  {**SAND, **OUTLINE, "base_color": [1.0, 0.86, 0.7], "tiling": [1.6, 1.6]},
    "clay":    {**OUTLINE, "base_color": [0.42, 0.17, 0.07], "roughness": 0.7, "rim": False,
                "specular": True, "shadow_tint": [0.45, 0.3, 0.3]},
    # Toon with a highlight, not lit metal: metal reflects the sky, and a tomb has none.
    "iron":    {"base_color": [0.07, 0.055, 0.045], "roughness": 0.4, "specular": True,
                "rim": False},
    "gold":    {**OUTLINE, "base_color": [0.85, 0.55, 0.14], "roughness": 0.3,
                "specular": True, "rim": False, "shadow_tint": [0.5, 0.35, 0.25]},
    "flame":   {"family": "emissive", "base_color": [1.0, 0.45, 0.1], "intensity": 3.0,
                "hot_core": False},
}
# Existing materials restyled (the maze wall boxes seen from above in the menu, the ground).
RESTYLE = {
    "wall":  {**SAND, "base_color": [0.6, 0.52, 0.45]},
    "floor": {"base_color": [0.06, 0.04, 0.03], "roughness": 0.9, "rim": False},
    # The grooved track (engine RailNetwork profile "grooved"): worn dark iron with a
    # highlight on the lips, the groove darker.
    "rail":        {"base_color": [0.2, 0.16, 0.13], "roughness": 0.35, "specular": True,
                    "rim": False, "shadow_tint": [0.45, 0.35, 0.3]},
    "rail_groove": {"base_color": [0.1, 0.08, 0.065], "roughness": 0.6, "rim": False},
}
# The railway's look: a wheel track like the reference image. The rails sit RAIL_Y above the
# floor (tools/maze_layout.py), so the sides go down that far (`depth`).
RAIL_Y = 0.12
RAILWAY = {"profile": "grooved", "material": "materials/rail.material.json",
           "groove_material": "materials/rail_groove.material.json", "depth": RAIL_Y,
           "groove_width": 0.26, "lip_width": 0.07, "lip_height": 0.06,
           "foot_width": 0.06, "foot_height": 0.06}
# Torch / fire-bowl light (baked PointLights at every flame).
TORCH_LIGHT = {"color": [1.0, 0.52, 0.2], "intensity": 1.6, "range": 7.5, "radius": 0.2}
BOWL_LIGHT = {"color": [1.0, 0.48, 0.16], "intensity": 1.0, "range": 7.0, "radius": 0.3}
FOG = {"fog": 1.0, "fog_start": 7.0, "fog_end": 34.0, "fog_color": [0.016, 0.009, 0.005],
       "fog_from_sky": False}


class Mesh:
    """Triangles per material slot; front faces counter-clockwise, flat normals."""

    def __init__(self):
        self.parts = {}

    def poly(self, mat, pts, n):
        pos, nor, idx = self.parts.setdefault(mat, ([], [], []))
        base = len(pos)
        pos.extend(pts)
        nor.extend([n] * len(pts))
        for i in range(1, len(pts) - 1):
            idx.extend([base, base + i, base + i + 1])

    def quad(self, mat, p, u, v):
        n = norm(cross(u, v))
        self.poly(mat, [p, add(p, u), add(add(p, u), v), add(p, v)], n)

    def box(self, mat, mn, mx, faces=None):
        """`faces`: the world normals to draw (default: all but the bottom). Hidden faces are
        left out: each face is a lightmap chart."""
        x0, y0, z0 = mn
        x1, y1, z1 = mx
        dx, dy, dz = (x1 - x0, 0, 0), (0, y1 - y0, 0), (0, 0, z1 - z0)
        f = faces if faces is not None else {(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, 0, 1),
                                              (0, 0, -1)}
        if (1, 0, 0) in f: self.quad(mat, (x1, y0, z0), dy, dz)
        if (-1, 0, 0) in f: self.quad(mat, (x0, y0, z0), dz, dy)
        if (0, 1, 0) in f: self.quad(mat, (x0, y1, z0), dz, dx)
        if (0, 0, 1) in f: self.quad(mat, (x0, y0, z1), dx, dy)
        if (0, 0, -1) in f: self.quad(mat, (x0, y0, z0), dy, dx)
        if (0, -1, 0) in f: self.quad(mat, (x0, y0, z0), dx, dz)

    def lathe(self, mat, cx, cz, y, profile, sides=8, spin=0.0):
        """Rings (radius, height) bottom to top; caps where the radius isn't 0."""
        rings = []
        for r, h in profile:
            rings.append([(cx + r * math.cos(spin + 2 * math.pi * k / sides), y + h,
                           cz + r * math.sin(spin + 2 * math.pi * k / sides))
                          for k in range(sides)])
        for a, b in zip(rings, rings[1:]):
            for k in range(sides):
                k1 = (k + 1) % sides
                quad = [a[k], b[k], b[k1], a[k1]]
                n = cross(sub(quad[1], quad[0]), sub(quad[2], quad[0]))
                if dot(n, n) < 1e-12:
                    n = cross(sub(quad[2], quad[0]), sub(quad[3], quad[0]))
                mid = [sum(c) / 4 for c in zip(*quad)]
                if dot(n, (mid[0] - cx, 0, mid[2] - cz)) < 0:
                    quad.reverse()
                    n = (-n[0], -n[1], -n[2])
                if dot(n, n) > 1e-12:
                    self.poly(mat, dedupe(quad), norm(n))
        if profile[-1][0] > 0:
            self.poly(mat, list(reversed(rings[-1])), (0, 1, 0))
        if profile[0][0] > 0:
            self.poly(mat, rings[0], (0, -1, 0))

    def tris(self):
        return sum(len(p[2]) for p in self.parts.values()) // 3


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    l = math.sqrt(dot(a, a)) or 1.0
    return (a[0] / l, a[1] / l, a[2] / l)


def dedupe(pts):
    out = []
    for p in pts:
        if not out or dot(sub(p, out[-1]), sub(p, out[-1])) > 1e-12:
            out.append(p)
    if len(out) > 1 and dot(sub(out[0], out[-1]), sub(out[0], out[-1])) < 1e-12:
        out.pop()
    return out


def write_glb(path, mesh):
    """One mesh, a primitive per material slot (named like the slot, for import mapping)."""
    blob, views, accessors, prims, mats = bytearray(), [], [], [], []

    def view(data, target):
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data),
                      "target": target})
        blob.extend(data)
        return len(views) - 1

    for i, (name, (pos, nor, idx)) in enumerate(sorted(mesh.parts.items())):
        r = lambda v: round(v, 4)
        pos = [tuple(map(r, p)) for p in pos]
        nor = [tuple(map(r, n)) for n in nor]
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
        c = MATERIALS[name]["base_color"]
        mats.append({"name": name, "pbrMetallicRoughness": {
            "baseColorFactor": c + [1.0], "metallicFactor": 0.0, "roughnessFactor": 0.9}})
    gltf = {"asset": {"version": "2.0", "generator": "coin-rush tools/environment.py"},
            "scene": 0, "scenes": [{"nodes": [0]}],
            "nodes": [{"mesh": 0, "name": os.path.basename(path)[:-4]}],
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


# --- the maze -------------------------------------------------------------------------------
project, out_dir = sys.argv[1], sys.argv[2]
content = os.path.join(project, "content")
scene_path = os.path.join(content, "scenes/main.scene.json")
scene = json.load(open(scene_path))
rails = json.load(open(os.path.join(content, "scenes/main.rails.json")))
nodes = {n["id"]: n for n in rails["nodes"]}
# Port angles (unrotated nodes): 0 north (-z), 90 west (-x), 180 south (+z), 270 east (+x).
ANGLE_DIR = {0: (0, -1), 90: (-1, 0), 180: (0, 1), 270: (1, 0)}


def node_dirs(n):
    return {ANGLE_DIR[int(round(a)) % 360] for a in n["ports"]}


corridors = []
for e in rails["edges"]:
    a, b = nodes[e["from"][0]], nodes[e["to"][0]]
    ax, az, bx, bz = a["position"][0], a["position"][2], b["position"][0], b["position"][2]
    assert ax == bx or az == bz, "corridors must be straight: " + e["id"]
    if (bx, bz) < (ax, az):
        a, b, ax, az, bx, bz = b, a, bx, bz, ax, az
    length = abs(bx - ax) + abs(bz - az)
    d = ((bx - ax) / length, (bz - az) / length)
    corridors.append({"a": a, "b": b, "o": (ax, az), "d": d, "s": (-d[1], d[0]), "L": length})

rng = random.Random(SEED)
order = list(range(len(corridors)))
rng.shuffle(order)
for k, i in enumerate(order):
    corridors[i]["theme"] = THEMES[k % len(THEMES)]


# Every mesh but the flames is Static, lit by the bake (the torch lamps). Every face is a
# lightmap chart, so the small details get a lower lightmap density (DETAIL_SCALE).
arch, props, trim, details = Mesh(), Mesh(), Mesh(), Mesh()     # Static.
flames = Mesh()                                                 # Emissive, live.
lamps = []                                                      # (name, position, PointLight)
DETAIL_SCALE = 0.4

class Frame:
    """Corridor-local coordinates: t along the rail from its first node, o across (+-HALF =
    the walls), y up; w = distance in from the wall on side `side` (+1 / -1)."""

    def __init__(self, c):
        self.c = c

    def world(self, t, o):
        (ox, oz), (dx, dz), (sx, sz) = self.c["o"], self.c["d"], self.c["s"]
        return (ox + dx * t + sx * o, oz + dz * t + sz * o)

    def dirs(self, side, names):
        """Local face names -> world normals: front (toward the rail), back (the wall),
        ends (along the rail), top, bottom."""
        if names is None:
            return None
        (dx, dz), (sx, sz) = self.c["d"], self.c["s"]
        v = lambda x, z: (int(round(x)), 0, int(round(z)))
        table = {"front": [v(-side * sx, -side * sz)], "back": [v(side * sx, side * sz)],
                 "ends": [v(dx, dz), v(-dx, -dz)], "top": [(0, 1, 0)], "bottom": [(0, -1, 0)]}
        return {n for name in names.split() for n in table[name]}

    def box(self, mesh, mat, t0, t1, o0, o1, y0, y1, faces=None):
        p, q = self.world(t0, o0), self.world(t1, o1)
        mesh.box(mat, (min(p[0], q[0]), y0, min(p[1], q[1])),
                 (max(p[0], q[0]), y1, max(p[1], q[1])), self.dirs(1, faces))

    def wbox(self, mesh, mat, side, t0, t1, w0, w1, y0, y1, faces=None):
        p = self.world(t0, side * (HALF - w0))
        q = self.world(t1, side * (HALF - w1))
        mesh.box(mat, (min(p[0], q[0]), y0, min(p[1], q[1])),
                 (max(p[0], q[0]), y1, max(p[1], q[1])), self.dirs(side, faces))

    def at(self, side, t, w):
        return self.world(t, side * (HALF - w))




def torch(f, side, t, w):
    """A wall torch on a face `w` in from the wall: iron bracket, cup, flame."""
    f.wbox(details, "iron", side, t - 0.09, t + 0.09, w - 0.02, w + 0.2, 2.42, 2.62)
    f.wbox(details, "iron", side, t - 0.05, t + 0.05, w - 0.02, w + 0.12, 2.1, 2.42)
    x, z = f.at(side, t, w + 0.26)
    details.lathe("iron", x, z, 2.56, [(0.07, 0), (0.12, 0.08), (0.19, 0.24)], 8)
    flames.lathe("flame", x, z, 2.74, [(0.14, 0), (0.15, 0.12), (0.0, 0.55)], 5)
    lamps.append(("Torch", (x, 3.0, z), TORCH_LIGHT))


def brazier(f, side, t):
    """A fire bowl on a stand on the floor."""
    f.wbox(details, "iron", side, t - 0.14, t + 0.14, 0.31, 0.59, 0.0, 0.62)
    f.wbox(details, "iron", side, t - 0.22, t + 0.22, 0.23, 0.67, 0.0, 0.08)
    x, z = f.at(side, t, 0.45)
    details.lathe("iron", x, z, 0.6, [(0.12, 0), (0.24, 0.1), (0.3, 0.24)], 8)
    flames.lathe("flame", x, z, 0.78, [(0.23, 0), (0.25, 0.15), (0.0, 0.75)], 5)
    lamps.append(("Fire Bowl", (x, 1.2, z), BOWL_LIGHT))


def blocks(f, side, t, rng):
    """A stack of fallen stone blocks against the wall."""
    w = rng.uniform(0.45, 0.65)
    a = rng.uniform(0.3, 0.55)
    h = rng.uniform(0.35, 0.7)
    tc = t + rng.uniform(-0.35, 0.35)
    f.wbox(props, "block", side, tc - a, tc + a, 0, w, 0, h, "front ends top")
    if rng.random() < 0.6:
        a2 = rng.uniform(0.25, a * 0.9)
        tc2 = tc + rng.uniform(-(a - a2), a - a2)
        f.wbox(props, "block", side, tc2 - a2, tc2 + a2, 0, w * rng.uniform(0.6, 0.9), h,
               h + rng.uniform(0.25, 0.5), "front ends top")
    if rng.random() < 0.5:
        s = rng.uniform(0.18, 0.28)
        t2 = tc + (a + s + 0.05) * rng.choice([-1, 1])
        f.wbox(details, "block", side, t2 - s, t2 + s, 0, 2 * s, 0, 2 * s, "front ends top")


def pots(f, side, t, rng):
    for k in range(rng.randint(1, 3)):
        s = rng.uniform(0.75, 1.25)
        tc = t + (k - 1) * 0.62 + rng.uniform(-0.1, 0.1)
        x, z = f.at(side, tc, 0.38)
        details.lathe("clay", x, z, 0.0, [(0.12 * s, 0), (0.25 * s, 0.13 * s),
                                          (0.29 * s, 0.32 * s), (0.22 * s, 0.52 * s),
                                          (0.11 * s, 0.62 * s), (0.14 * s, 0.7 * s)], 8,
                      rng.random())


def statue(f, side, t):
    """A pharaoh guard: pedestal, robe, torso, crossed arms (gold), head with a headdress."""
    S, vis = side, "front ends top"
    f.wbox(props, "block", S, t - 0.45, t + 0.45, 0, 0.64, 0, 0.5, vis)
    f.wbox(props, "statue", S, t - 0.26, t + 0.26, 0.08, 0.5, 0.5, 1.95, "front ends")
    f.wbox(props, "statue", S, t - 0.37, t + 0.37, 0.06, 0.52, 1.95, 2.72, vis)
    f.wbox(props, "statue", S, t - 0.15, t + 0.15, 0.17, 0.46, 2.78, 3.18, "front ends")
    f.wbox(details, "gold", S, t - 0.31, t + 0.31, 0.04, 0.56, 2.15, 2.33, "front ends top bottom")
    f.wbox(details, "gold", S, t - 0.1, t + 0.1, 0.06, 0.55, 1.2, 1.95, "front ends")
    f.wbox(details, "statue", S, t - 0.1, t + 0.1, 0.2, 0.4, 2.72, 2.8, "front ends")
    f.wbox(details, "gold", S, t - 0.25, t + 0.25, 0.1, 0.4, 2.72, 3.3, vis)
    f.wbox(details, "gold", S, t - 0.17, t + 0.17, 0.14, 0.38, 3.3, 3.4, vis)


def glyph_panel(f, side, t, rng):
    """A carved panel (the glyph texture) in a stone frame."""
    f.wbox(props, "glyph", side, t - 0.85, t + 0.85, 0, 0.09, 0.85, 3.15, "front ends")
    f.wbox(props, "trim", side, t - 0.95, t + 0.95, 0, 0.13, 3.15, 3.3, "front ends top bottom")
    f.wbox(props, "trim", side, t - 0.95, t + 0.95, 0, 0.13, 0.7, 0.85, "front ends top")


def steps(f, side, t):
    for k, (a, w) in enumerate([(0.95, 0.66), (0.68, 0.46), (0.4, 0.26)]):
        f.wbox(props, "block", side, t - a, t + a, 0, w, 0.3 * k, 0.3 * (k + 1), "front ends top")


# --- build ----------------------------------------------------------------------------------
for ci, c in enumerate(corridors):
    f = Frame(c)
    L, theme = c["L"], c["theme"]
    rng = random.Random(SEED * 1000 + ci)
    pillar_mat = "stone" if ci % 2 else "stone_b"
    # Wall courses: skirting, two stone bands, cornice. A side runs on to the node where the
    # node has no exit that way (T junctions, corners), so the courses stay unbroken.
    for side in (1, -1):
        ext = []
        for node, t_end, sign in ((c["a"], 0.0, -1), (c["b"], L, 1)):
            sd = (round(c["s"][0] * side), round(c["s"][1] * side))
            if sd in node_dirs(node):
                ext.append(t_end - sign * HALF)       # Stops at the block's corner.
            elif len(node["ports"]) == 2:
                ext.append(t_end + sign * HALF)       # Outer corner: up to the next wall.
            else:
                ext.append(t_end)                     # Wall runs on past the T junction.
        t0, t1 = ext
        # The lit wall face (the "Wall NN" box behind it is not Static: its tops, ends and
        # back faces would fill the lightmap without ever being seen from the tunnel).
        f.wbox(arch, "wall", side, t0, t1, WALL_SKIN, WALL_SKIN, 0, WALL_H, "front")
        f.wbox(trim, "trim", side, t0, t1, 0, 0.1, 0, 0.35, "front top ends")
        f.wbox(trim, "trim", side, t0, t1, 0, 0.07, 1.6, 1.74, "front top bottom ends")
        f.wbox(trim, "trim", side, t0, t1, 0, 0.07, 3.2, 3.34, "front top bottom ends")
        f.wbox(trim, "trim", side, t0, t1, 0, 0.2, 5.2, CEIL_Y, "front bottom ends")
    # Heavy lintels where the corridor opens into a junction.
    for tl in (HALF, L - HALF):
        f.box(arch, "stone_b", tl - 0.3, tl + 0.3, -HALF, HALF, 4.45, CEIL_Y, "ends bottom")
    # Stations: pillar pairs + a beam; the middle one of a long corridor is a gate (pylons).
    n = int(round((L - 2 * END) / STATION)) + 1
    ts = [END + k * (L - 2 * END) / (n - 1) for k in range(n)]
    gate = n // 2 if (L >= 18 and theme in ("hall", "gallery", "steps")) else -1
    for k, t in enumerate(ts):
        heavy = k == gate
        wf = 0.72 if heavy else 0.45   # Pillar front face, in from the wall.
        hw = 0.7 if heavy else 0.4
        for side in (1, -1):
            f.wbox(arch, pillar_mat, side, t - hw, t + hw, 0, wf, 0.45, 4.1, "front ends")
            f.wbox(arch, "trim", side, t - hw - 0.1, t + hw + 0.1, 0, wf + 0.1, 0, 0.45,
                   "front ends top")
            f.wbox(arch, "trim", side, t - hw - 0.12, t + hw + 0.12, 0, wf + 0.14, 4.1, 4.6,
                   "front ends bottom")
            if heavy:
                f.wbox(trim, "trim", side, t - hw + 0.1, t + hw - 0.1, wf, wf + 0.03, 1.2, 3.6,
                       "front ends top bottom")
        bw = 0.6 if heavy else 0.32
        f.box(arch, "stone_b" if heavy else pillar_mat, t - bw, t + bw, -HALF, HALF,
              4.45 if heavy else 4.6, CEIL_Y, "ends bottom")
        # Torches: on every other station; both sides in halls and at gates.
        if k % 2 == 0 or heavy:
            both = theme in ("hall", "gallery") or heavy
            for side in ((1, -1) if both else ((1,) if k % 4 == 0 else (-1,))):
                torch(f, side, t, wf + 0.14 if not heavy else wf)
    # Bays between pillars: the theme's props.
    for k in range(n - 1):
        tb = (ts[k] + ts[k + 1]) / 2
        for side in (1, -1):
            r = rng.random()
            if theme == "gallery" and k % 2 == 0:
                statue(f, side, tb)
            elif theme == "glyphs":
                glyph_panel(f, side, tb, rng)
                if (k + (side > 0)) % 2 == 0:
                    brazier(f, side, tb)
            elif theme == "storage" and r < 0.55:
                pots(f, side, tb, rng)
            elif theme == "steps" and r < 0.5:
                steps(f, side, tb)
            elif r < (0.35 if theme == "gallery" else 0.5):
                blocks(f, side, tb, rng)
    # The corridor's floor and ceiling (lightmapped; the ground plane under the walls is not).
    # The ceiling faces down only, so the top-down menu view still looks into the maze.
    f.box(arch, "tile", HALF, L - HALF, -HALF, HALF, 0.0, 0.003, "top")
    f.box(arch, "ceiling", HALF, L - HALF, -HALF, HALF, CEIL_Y - 0.003, CEIL_Y, "bottom")

# Junction squares' floors and ceilings.
for n in nodes.values():
    x, z = n["position"][0], n["position"][2]
    arch.box("tile", (x - HALF, 0.0, z - HALF), (x + HALF, 0.003, z + HALF), {(0, 1, 0)})
    arch.box("ceiling", (x - HALF, CEIL_Y - 0.003, z - HALF), (x + HALF, CEIL_Y, z + HALF),
             {(0, -1, 0)})

models = os.path.join(content, "models/env")
os.makedirs(models, exist_ok=True)
os.makedirs(os.path.join(content, "materials/env"), exist_ok=True)
MESHES = {"architecture": arch, "props": props, "flames": flames, "trim": trim,
          "details": details}
for old in ("grout.glb", "grout.glb.import.json"):     # Made by an earlier version.
    if os.path.exists(os.path.join(models, old)):
        os.remove(os.path.join(models, old))
if os.path.exists(os.path.join(content, "materials/env/grout.material.json")):
    os.remove(os.path.join(content, "materials/env/grout.material.json"))
for name, mesh in MESHES.items():
    write_glb(os.path.join(models, name + ".glb"), mesh)
    imp = {"type": "model_import", "version": 1,
           "materials": {m: f"materials/env/{m}.material.json" for m in sorted(mesh.parts)}}
    json.dump(imp, open(os.path.join(models, name + ".glb.import.json"), "w"), indent=2)
    print(f"{name}: {mesh.tris()} triangles, slots {sorted(mesh.parts)}")
for name, fields in MATERIALS.items():
    m = {"type": "material", "version": 2, **fields}
    json.dump(m, open(os.path.join(content, f"materials/env/{name}.material.json"), "w"),
              indent=2)
for name, fields in RESTYLE.items():
    json.dump({"type": "material", "version": 2, **fields},
              open(os.path.join(content, f"materials/{name}.material.json"), "w"), indent=2)

# --- scene commands -------------------------------------------------------------------------
ids = {e["name"]: e["id"] for e in scene["entities"]}
scene_floor = next(e for e in scene["entities"] if e["name"] == "Floor")
# Deleting a group deletes its children too.
cmds = [{"cmd": "entity.delete", "id": e["id"]} for e in scene["entities"]
        if e["id"].startswith("e_e9a7") and not e.get("parent", "").startswith("e_e9a7")]
for e in scene["entities"]:
    if e["name"].startswith("Wall "):
        p = e["components"]["Transform"]["position"]
        s = e["components"]["Transform"]["scale"]
        cmds.append({"cmd": "component.set", "id": e["id"], "component": "Transform",
                     "value": {"position": [p[0], WALL_H / 2, p[2]],
                               "scale": [s[0], WALL_H / 2, s[2]]}})
        if "Static" in e["components"]:  # Env Architecture holds the lit wall faces.
            cmds.append({"cmd": "component.remove", "id": e["id"], "component": "Static"})


def entity(n, name, comps):
    cmds.append({"cmd": "entity.create",
                 "entity": {"id": f"e_e9a7{n:04x}", "name": name, "components": comps}})


for i, name in enumerate(MESHES):
    comps = {"Transform": {}, "Mesh": {"mesh": f"models/env/{name}.glb"}}
    if name != "flames":
        comps["Static"] = {"lightmap_scale": DETAIL_SCALE} if name == "details" else {}
    entity(2 + i, f"Env {name.capitalize()}", comps)
# Dark, warm tomb air: no sun (a dungeon), a dim warm fill, fog swallowing the far end of each
# corridor; the torch lamps light it (in the bake).
entity(10, "Atmosphere", {
    "Look": {"exposure": 0.6, "contrast": 1.12, "saturation": 0.95, "warmth": 0.0,
             "vignette": 0.4, "bloom": 0.5, **FOG},
    "BakedLighting": {"texels_per_meter": 8, "max_size": 1400, "samples": 128, "bounces": 2, "sun_samples": 16,
                      "denoise": True, "ao_strength": 0.7, "ao_distance": 0.8}})
# The lamps, grouped under "Env Lights" (the Lighting window lists them).
entity(11, "Env Lights", {"Transform": {}})
for k, (name, p, light) in enumerate(lamps):
    cmds.append({"cmd": "entity.create", "entity": {
        "id": f"e_e9a7{0x100 + k:04x}", "name": f"{name} {k + 1:03d}", "parent": "e_e9a7000b",
        "components": {"Transform": {"position": [round(v, 3) for v in p]},
                       "PointLight": dict(light)}}})
light = next(e for e in scene["entities"] if "AmbientLight" in e["components"])
cmds.append({"cmd": "component.set", "id": light["id"], "component": "AmbientLight",
             "value": {"color": AMBIENT_COLOR, "intensity": AMBIENT}})
if "DirectionalLight" in light["components"]:  # No sun underground (engine: no sun at all).
    cmds.append({"cmd": "component.remove", "id": light["id"], "component": "DirectionalLight"})
if light["name"] != "Ambient Light":
    cmds.append({"cmd": "entity.rename", "id": light["id"], "name": "Ambient Light"})
railway = next(e for e in scene["entities"] if "RailNetwork" in e["components"])
cmds.append({"cmd": "component.set", "id": railway["id"], "component": "RailNetwork",
             "value": RAILWAY})
if "Static" in scene_floor["components"]:  # The ground under the walls needs no lightmap.
    cmds.append({"cmd": "component.remove", "id": ids["Floor"], "component": "Static"})
json.dump(cmds, open(os.path.join(out_dir, "scene.json"), "w"), indent=1)
print(f"{len(corridors)} corridors, {len(lamps)} lamps, "
      f"themes {sorted((c['theme'] for c in corridors))}")
