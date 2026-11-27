#!/usr/bin/env python3
"""Coin Rush's maze: the layout below -> rt commands for the railway, the walls and the maze
spots (hunter homes, player spawns). Rules: docs/gameplay.md ("The arena"), docs/maze.md.

Usage (from the engine root):
  python3 games/coin-rush/tools/maze_layout.py games/coin-rush/content/scenes/main.scene.json OUT
  -> OUT/rails.json (rail.* commands for a fresh main.rails.json) and OUT/scene.json
     (entity commands for main.scene.json). docs/maze.md has the full rebuild steps.

Layout: one character = 3 m. '+' = rail node (a junction, or a two-port corner robots drive
through), '-' and '|' = rails, anything else = wall. Must be mirrored left to right."""
import json, sys

LAYOUT = """
+-----+-----+-----+
|     |     |     |
|     |     |     |
|     |     |     |
|     |     |     |
|     |     |     |
|     |     |     |
|     |     |     |
+-----+-----+-----+
|     |     |     |
|     |     |     |
|     |     |     |
+-----+     +-----+
|     |     |     |
|     |     |     |
|     |     |     |
|     |     |     |
|     |     |     |
+-----+-----+-----+
"""
CELL = 3.0          # Metres per character.
RAIL_Y = 0.3        # Rail height (ground is y = 0).
WALL_GAP = 2.0      # Walls stop this far from a rail's centre line (junction curves cut 1.2 m in).
WALL_HEIGHT = 5.6   # Tunnel walls up to the ceiling (tools/environment.py builds the rest).
BORDER = 1.0        # Thickness of the outer wall.
# Hunter homes, one per corner (north = -z = top of the layout), like Pac-Man's scatter corners.
HOMES = {"pinky": (0, 0), "blinky": (-1, 0), "clyde": (0, -1), "inky": (-1, -1)}
# Player spawns: (column, row) of intersections around the centre.
SPAWNS = [(6, 8), (12, 8), (6, 12), (12, 12)]
START = (6, 12)     # Where the player robot starts until runs pick a spawn (engine request #3).

# Ports by direction for an unrotated node (forward = -z): 0 north, 90 west, 180 south, 270 east.
PORT_ANGLE = {"N": 0, "W": 90, "S": 180, "E": 270}
STEP = {"N": (0, -1), "S": (0, 1), "E": (1, 0), "W": (-1, 0)}
OPPOSITE = {"N": "S", "S": "N", "E": "W", "W": "E"}

rows = [r for r in LAYOUT.strip("\n").split("\n")]
W, H = max(len(r) for r in rows), len(rows)
rows = [r.ljust(W) for r in rows]


def at(c, r):
    return rows[r][c] if 0 <= r < H and 0 <= c < W else " "


def world(c, r):
    return [round((c - (W - 1) / 2) * CELL, 3), RAIL_Y, round((r - (H - 1) / 2) * CELL, 3)]


def has_rail(c, r, d):
    dc, dr = STEP[d]
    ch = at(c + dc, r + dr)
    return ch == "+" or (ch == "-" and dr == 0) or (ch == "|" and dc == 0)


for r in rows:
    assert r == r[::-1], "the layout must be mirrored left to right: " + r

# Nodes: fixed ids so later commands (and scenes) can name them.
nodes = {}
for r in range(H):
    for c in range(W):
        if at(c, r) == "+":
            dirs = [d for d in "NWSE" if has_rail(c, r, d)]
            assert len(dirs) >= 2, f"dead end at column {c}, row {r}"
            nodes[(c, r)] = {"id": f"n_3a2e{len(nodes) + 1:04x}", "dirs": dirs}

rail_cmds = []
for (c, r), n in nodes.items():
    rail_cmds.append({"cmd": "rail.add_node", "node": {
        "id": n["id"], "position": world(c, r), "rotation": [0, 0, 0],
        "ports": [PORT_ANGLE[d] for d in n["dirs"]]}})
edges = 0
for (c, r), n in nodes.items():
    for d in ("E", "S"):  # Each rail once: from its west / north end.
        if d not in n["dirs"]:
            continue
        dc, dr = STEP[d]
        cc, rr = c + dc, r + dr
        while at(cc, rr) != "+":
            cc, rr = cc + dc, rr + dr
        other = nodes[(cc, rr)]
        edges += 1
        rail_cmds.append({"cmd": "rail.connect", "edge": {
            "id": f"r_3a2e{edges:04x}",
            "from": [n["id"], n["dirs"].index(d)],
            "to": [other["id"], other["dirs"].index(OPPOSITE[d])]}})

# Walls: every enclosed block of non-rail characters becomes one box, WALL_GAP from the rails.
seen, blocks = set(), []
for r in range(H):
    for c in range(W):
        if at(c, r) in "+-|" or (c, r) in seen:
            continue
        todo, cells = [(c, r)], []
        seen.add((c, r))
        while todo:
            x, y = todo.pop()
            cells.append((x, y))
            for dx, dy in STEP.values():
                p = (x + dx, y + dy)
                if 0 <= p[0] < W and 0 <= p[1] < H and at(*p) not in "+-|" and p not in seen:
                    seen.add(p)
                    todo.append(p)
        xs, ys = [p[0] for p in cells], [p[1] for p in cells]
        assert len(cells) == (max(xs) - min(xs) + 1) * (max(ys) - min(ys) + 1), \
            "wall blocks must be rectangles"
        # Bounded by the rails one character outside the block.
        x0, z0 = world(min(xs) - 1, min(ys) - 1)[0], world(min(xs) - 1, min(ys) - 1)[2]
        x1, z1 = world(max(xs) + 1, max(ys) + 1)[0], world(max(xs) + 1, max(ys) + 1)[2]
        blocks.append((x0 + WALL_GAP, z0 + WALL_GAP, x1 - WALL_GAP, z1 - WALL_GAP))
# The outer wall: four boxes around the whole layout.
X, Z = world(W - 1, H - 1)[0], world(W - 1, H - 1)[2]
o = WALL_GAP + BORDER
blocks += [(-X - o, -Z - o, X + o, -Z - WALL_GAP), (-X - o, Z + WALL_GAP, X + o, Z + o),
           (-X - o, -Z - WALL_GAP, -X - WALL_GAP, Z + WALL_GAP),
           (X + WALL_GAP, -Z - WALL_GAP, X + o, Z + WALL_GAP)]


def r3(v):
    return round(v, 3)


scene_path, out_dir = sys.argv[1], sys.argv[2]
scene = json.load(open(scene_path))
scene_cmds = []
# Delete what an earlier run made (walls, spots), then make it again with the same ids.
for e in scene["entities"]:
    if e["id"].startswith("e_3a2e"):
        scene_cmds.append({"cmd": "entity.delete", "id": e["id"]})
for i, (x0, z0, x1, z1) in enumerate(blocks):
    scene_cmds.append({"cmd": "entity.create", "entity": {
        "id": f"e_3a2e{0x1000 + i:04x}", "name": f"Wall {i + 1:02d}", "components": {
            "Transform": {"position": [r3((x0 + x1) / 2), WALL_HEIGHT / 2, r3((z0 + z1) / 2)],
                          "scale": [r3((x1 - x0) / 2), WALL_HEIGHT / 2, r3((z1 - z0) / 2)]},
            "Mesh": {"mesh": "cube", "material": "materials/wall.material.json"},
            "Static": {}}}})
spots = [(f"Home {h.capitalize()}", h, (cc % W, rr % H)) for h, (cc, rr) in HOMES.items()]
spots += [(f"Spawn {i + 1}", "spawn", p) for i, p in enumerate(SPAWNS)]
for i, (name, role, (c, r)) in enumerate(spots):
    assert (c, r) in nodes, f"{name}: no node at column {c}, row {r}"
    pad = 1.2 if role == "spawn" else 2.0
    x, _, z = world(c, r)
    scene_cmds.append({"cmd": "entity.create", "entity": {
        "id": f"e_3a2e{0x2000 + i:04x}", "name": name, "components": {
            # A flat pad under the node, so homes and spawns show in renders and the editor.
            "Transform": {"position": [x, 0.02, z], "scale": [pad, 0.02, pad]},
            "Mesh": {"mesh": "cube", "material": f"materials/{'spawn' if role == 'spawn' else 'home_' + role}.material.json"},
            "MazeSpot": {"role": role, "node": nodes[(c, r)]["id"]}}}})
# Floor, camera (top-down on the whole maze) and the player's start.
ids = {e["name"]: e["id"] for e in scene["entities"]}
half = max(X, Z) + o + 2
scene_cmds += [
    {"cmd": "component.set", "id": ids["Floor"], "component": "Transform",
     "value": {"scale": [r3(half), 1.0, r3(half)]}},
    {"cmd": "component.set", "id": ids["Main Camera"], "component": "Transform",
     "value": {"position": [0.0, 95.0, 22.0], "rotation": [-77.0, 0.0, 0.0]}},
]
sx, _, sz = world(*START)
start = nodes[START]
scene_cmds += [
    {"cmd": "component.set", "id": ids["Player"], "component": "Transform",
     "value": {"position": [sx, 0.38, sz], "rotation": [0.0, 0.0, 0.0]}},
    {"cmd": "component.set", "id": ids["Player"], "component": "Robot",
     "value": {"start_node": start["id"], "start_port": start["dirs"].index("S")}},
]
json.dump(rail_cmds, open(f"{out_dir}/rails.json", "w"), indent=1)
json.dump(scene_cmds, open(f"{out_dir}/scene.json", "w"), indent=1)
print(f"{len(nodes)} nodes, {edges} rails, {len(blocks)} walls, {len(spots)} spots")
