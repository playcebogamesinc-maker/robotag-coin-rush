# The level look: Egyptian pyramid tunnels

Built from basic shapes (boxes and low-poly turned shapes), matching Vlad's reference images
(Nov 27 2026) and "Look and feel" in [gameplay.md](gameplay.md). Static scenery only: rails,
coins, robots and UI are untouched.

## What's there

- **Tunnel:** the maze walls ("Wall NN", [maze.md](maze.md)) are 5.6 m tall sandstone and a
  ceiling closes the tunnel at 5.6 m. The ceiling faces down only, so the top-down menu view
  still looks into the maze.
- **Every corridor:** pillar pairs every ~3 m (base, shaft, capital) with a ceiling beam, heavy
  lintels where it opens into a junction, stone courses along the walls (skirting, two bands,
  cornice), a tiled floor (1 m tiles; the rails sit on tile seams).
- **Torches:** iron bracket, cup and flame on every other pillar pair. The flames are an
  emissive material, and the light bake treats emissive Static meshes as lights: the torches
  light the walls, floor, props and (through the rail probes) the robots. No real-time lights.
- **Themes** (each corridor gets one, so neighbours differ, like the second reference):
  `hall` (torches on both sides, fallen blocks, a gate of heavy pylons in the middle),
  `gallery` (pharaoh guard statues facing each other, gold trim), `storage` (clay pots),
  `glyphs` (hieroglyph wall panels and floor fire bowls), `steps` (stepped blocks, a gate).
- **Atmosphere:** dim warm ambient, a very dim sun, an `Atmosphere` entity with the `Look`
  (warm, a bit brighter, vignette, bloom on the flames) and `BakedLighting` settings.
  Not yet: fog (distant corridors fading to dark) and out-of-step flicker, engine request #10
  ([engine-requests/fog.md](engine-requests/fog.md)).

Clearance: nothing comes closer than 1.05 m to a rail's centre line (torch cups; pillars
1.4 m, props 1.3 m), the junction squares are empty, beams and lintels start at 4.45 m (the
follow camera rides about 3.9 m up).

## How it's built

`tools/environment.py` reads the maze (the "Wall NN" boxes and `main.rails.json`) and writes:

| File / entity | What | Lit by |
|---|---|---|
| `models/env/architecture.glb` (Env Architecture) | pillars, beams, lintels, corridor floors | bake (Static) |
| `models/env/trim.glb` (Env Trim) | wall courses, gate inlays | bake (Static) |
| `models/env/props.glb` (Env Props) | blocks, steps, statues, glyph panels | bake (Static) |
| `models/env/flames.glb` (Env Flames) | torch and fire-bowl flames (emissive) | itself; lights the bake |
| `models/env/details.glb` (Env Details) | small things: iron, pots, gold, glyph marks | live (ambient + dim sun) |
| `models/env/grout.glb` (Env Grout) | tile seams | unlit |
| `materials/env/*.material.json` | one per slot (the `.glb.import.json` maps them) | |
| Ceiling, Atmosphere, Sun, Floor | scene entities / settings | |

Everything is merged into six meshes: six draws instead of thousands (the renderer has no
instancing or culling yet), about 45k triangles in all. Only big, simple surfaces are Static,
because every face of a Static mesh is a lightmap chart, and thousands of tiny charts crowd the
1024 x 1024 atlas (the phone budget): the bake now lands at about 4 texels per metre. Hidden
faces (against walls, floor, ceiling) are left out for the same reason.

## Change it

Tune the constants at the top of `tools/environment.py` (heights, spacing, `THEMES`,
`MATERIALS`, `SEED` for a different theme shuffle), then from the engine root (Windows:
`rt.cmd`, `python`):

```
python3 games/coin-rush/tools/environment.py games/coin-rush /tmp
build/linux/bin/Debug/rt apply games/coin-rush/content/scenes/main.scene.json /tmp/scene.json
build/linux/bin/Debug/rt validate games/coin-rush
build/linux/bin/Debug/rt bake games/coin-rush/content/scenes/main.scene.json
```

The script replaces what it made before (entity ids `e_e9a7...`) and leaves the rest alone.
After a maze change (`tools/maze_layout.py`), run it again.

## Seeing it

The torch light lives in the bake. In the editor: **Window > Lighting > Bake** once after
pulling (the Android build bakes by itself). Without a bake the tunnels show only the dim
ambient light. The bake is generated data (`.cache/bake/`), never committed.
