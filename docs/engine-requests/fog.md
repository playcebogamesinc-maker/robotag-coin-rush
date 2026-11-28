# Engine request #10: distance fog (and torch flicker) for Coin Rush's tunnels

**Status (Nov 28 2026):** A (fog) is done in the engine (M20 step 6, `Look.fog`, `fog_start`,
`fog_end`, `fog_color`, height fog) and Coin Rush uses it. B (flicker) is still open: the
torch light is baked `PointLight`s now, so flicker would mean a live light term or a per-object
emissive phase.

Paste this into the Robotag: Game Engine project. Small: three fields on `Look` and one line in
the shared colour finish. Coin Rush's level look (Egyptian pyramid tunnels, `docs/environment.md`)
is done in the game repo; this is the one thing it can't do itself.

## Why

The tunnels are long straight corridors lit only by torches (baked emissive light). Vlad's
reference shows the far end of every corridor fading into warm darkness: it hides how far the
maze goes, makes the torches pop and gives depth. Today the far end of a corridor is as sharp as
the near end, and the engine has no fog: nothing in `Look`, `scene.schema` or the shaders
(`finish_color` in `mesh_common.slang`).

## What the game needs

### A. Distance fog (needed)

Three fields on the existing `Look` component (scene.schema), so no new component and no
migration (new fields with defaults):

```
// Look
fog_color: color = [0, 0, 0]                        // Linear RGB the far scene fades to.
fog_start: f32   = 0    @unit("m") @min(0)          // No fog closer than this to the camera.
fog_end:   f32   = 0    @unit("m") @min(0)          // Full fog here; 0 = fog off (default).
```

- Linear fog by view distance (camera to the pixel), `mix(color, fog_color, saturate((d -
  start) / (end - start)))`, applied before the tonemap / look math, so it's the same on every
  quality profile (Low in the material shaders, Medium / High too: the same `finish_color` path
  as the Look today).
- On every 3D family: toon, lit, unlit, emissive (torches fade out with distance too), rails,
  robots, transparent and particles (additive ones fade to nothing, not to `fog_color`).
- Never on the UI, and the bloom threshold sees the fogged colour (far torches don't glow).
- Cost: one distance and one mix per pixel (or per vertex on Low if that's cheaper), fine on
  phones.
- `rt render` and the editor's Game view show it; the Inspector shows the fields under Look.

Coin Rush would use roughly `fog_color` = very dark brown `[0.02, 0.012, 0.007]`,
`fog_start` = 8 m, `fog_end` = 30 m (corridors are 12 to 24 m long; the follow camera sits 6 m
behind the robot).

### B. Torch flicker (nice to have, not blocking)

Coin Rush has 128 torch and brazier flames in one emissive material. `pulse_speed` makes them
all pulse in sync, which looks mechanical, so they're steady today. Wanted: a per-object phase
(e.g. derived from the world position in the emissive shader, or a `pulse_jitter` field on the
emissive family) so flames in one merged mesh flicker out of step. The bake keeps using the
steady intensity.

## Not needed

- Real-time point lights: the bake already turns emissive Static meshes into light (the
  torches light the walls, floor and props, and the rail probes light the robots), which is
  cheaper on phones than live lights.
- New primitives (cylinders): the game generates its low-poly shapes into .glb files.

## How Coin Rush will use it

`tools/environment.py` sets the fog on its `Atmosphere` entity's `Look`; rerun it and the
scene picks it up. No game code change.
