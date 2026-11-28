# The level look: Egyptian pyramid tunnels

Built from basic shapes (boxes and low-poly turned shapes) with stone textures, matching Vlad's
reference images (Nov 27 2026) and "Look and feel" in [gameplay.md](gameplay.md). Static
scenery only: rails, coins, robots and UI are untouched. Uses the engine's M19 / M20 features
(baked lamps, directional lightmaps, denoiser, fog, textures, corner darkening, outlines).

## What's there

- **Tunnel:** the maze walls ("Wall NN", [maze.md](maze.md)) are 5.6 m tall and a ceiling
  closes the tunnel at 5.6 m. The ceiling faces down only, so the top-down menu view still
  looks into the maze. The lit wall faces are part of Env Architecture, 2 cm in front of the
  "Wall NN" boxes (which stay for the map and the menu view, not Static).
- **Every corridor:** pillar pairs every ~3 m (base, shaft, capital) with a ceiling beam, heavy
  lintels where it opens into a junction, stone courses along the walls (skirting, two bands,
  cornice), a floor of 1 m slabs (the rails sit on the slab seams).
- **Stone:** three tileable textures with normal maps in `content/textures/env/` (made by
  `tools/textures.py`): `sandstone` (ashlar blocks in 0.5 m courses: walls, pillars, trim,
  props, ceiling; triplanar), `slabs` (floor; projected from above, seams on whole metres) and
  `glyphs` (carved hieroglyph columns on the glyph panels). Each image covers 2 x 2 m
  (`tiling` 0.5). Normal maps show on Medium / High (phones run Medium), not on Low.
- **Torches:** iron bracket, cup and flame on every other pillar pair, fire bowls in the glyph
  corridors. Each flame has a baked `PointLight` (warm, range 7.5 m; fire bowls 7 m), 128 in
  all, grouped under "Env Lights". The flames themselves are emissive (they glow and bloom,
  but don't light the bake). No real-time lights: the lamps light the lightmaps (with soft
  shadows, directions for toon light steps) and the rail probes (robots and rails).
- **Themes** (each corridor gets one, so neighbours differ, like the second reference):
  `hall` (torches on both sides, fallen blocks, a gate of heavy pylons in the middle),
  `gallery` (pharaoh guard statues facing each other, gold), `storage` (clay pots),
  `glyphs` (carved hieroglyph panels and floor fire bowls), `steps` (stepped blocks, a gate).
  Props (blocks, statues, pots, gold) have a thin dark outline so they read at a glance.
- **Atmosphere:** no sun (a dungeon: the scene has no DirectionalLight), a dim cool fill
  (`AMBIENT`, `AMBIENT_COLOR`) against the warm torches, an `Atmosphere` entity with the `Look`
  (contrast, vignette, bloom on the flames, **fog**: the far end of every corridor fades into
  warm darkness from 7 m to 34 m) and `BakedLighting` (denoiser, corner darkening `ao_*`,
  `max_size` 1400).
  Not yet: torch flicker (lamps are baked only; engine request #10 part B).

Clearance: nothing comes closer than 1.05 m to a rail's centre line (torch cups; pillars
1.4 m, props 1.3 m), the junction squares are empty, beams and lintels start at 4.45 m (the
follow camera rides about 3.9 m up).

## How it's built

`tools/environment.py` reads the maze (the "Wall NN" boxes and `main.rails.json`) and writes:

| File / entity | What | Lighting |
|---|---|---|
| `models/env/architecture.glb` (Env Architecture) | wall faces, pillars, beams, lintels, floors, ceilings | bake (Static) |
| `models/env/trim.glb` (Env Trim) | wall courses, gate inlays | bake (Static) |
| `models/env/props.glb` (Env Props) | blocks, steps, statues, glyph panels | bake (Static) |
| `models/env/details.glb` (Env Details) | small things: iron, pots, gold, small blocks | bake (Static, `lightmap_scale` 0.4) |
| `models/env/flames.glb` (Env Flames) | torch and fire-bowl flames | emissive (glows) |
| Env Lights (128 `PointLight`s) | the torch light | baked |
| `materials/env/*.material.json` | one per slot (the `.glb.import.json` maps them) | |
| Atmosphere, Ambient Light, Floor | scene entities / settings | |

Everything is merged into five meshes: five draws instead of thousands (the renderer has no
instancing or culling yet), about 25k triangles in all. Every face of a Static mesh is a
lightmap chart, so hidden faces (against walls, floor, ceiling) are left out and the small
details get a lower lightmap density. The bake fits 1400 x 1400 (about 6.8 texels per metre,
3.5 MB on a phone with ASTC, inside the 4 MB budget). Lightmap UVs are made by the engine
(xatlas) when it bakes; the .glb files carry none. Textures are 6 x 512 px (about 2 MB on a
phone).

## Change it

Tune the constants at the top of `tools/environment.py` (heights, spacing, `THEMES`,
`MATERIALS`, `TORCH_LIGHT`, `FOG`, `SEED` for a different theme shuffle), then from the engine
root (Windows: `rt.cmd`, `python`):

```
python3 games/coin-rush/tools/environment.py games/coin-rush /tmp
build/linux/bin/Debug/rt apply games/coin-rush/content/scenes/main.scene.json /tmp/scene.json
build/linux/bin/Debug/rt validate games/coin-rush
build/linux/bin/Debug/rt bake games/coin-rush/content/scenes/main.scene.json --device gpu
```

The script replaces what it made before (entity ids `e_e9a7...`) and leaves the rest alone.
After a maze change (`tools/maze_layout.py`), run it again. The textures change only with
`python3 games/coin-rush/tools/textures.py games/coin-rush` (needs numpy and Pillow); the PNGs
are committed.

## Seeing it

The torch light lives in the bake. In the editor: **Window > Lighting**, preset **Final**,
**Bake** once after pulling (GPU; the Android build bakes by itself). Without a bake the
tunnels show only the dim fill light. The view must be back on **Lit** (the "Baked light" view
shows the bake on white surfaces). The bake is generated data (`.cache/bake/`), never committed.
