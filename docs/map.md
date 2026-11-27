# The hold-to-view map (milestone 6)

Design: [gameplay.md](gameplay.md) "HUD and map". Code: `code/core/map.hpp` (the drawing),
the `Run` system in `code/core/coin_rush.cpp` (open / shut, models), `content/ui/map.tsx`
(the overlay) and `content/ui/hud.tsx` (the button).

## How it plays

- A **MAP** button sits at the bottom middle of the HUD, under the premove arrow. Holding it
  shows the full-screen map over a dark see-through backdrop; letting go shuts it.
- While it is open the robot **keeps driving but takes no input**: swipes do nothing. A premove
  queued before opening is still taken; without one the robot stops at the next intersection,
  as it would with no swipe. That is the risk of checking the map.
- The pause button hides while the map is open. Pausing (Escape / P), a catch or the end of the
  run shuts the map. It only opens in play.

## What it shows

North up, 7 dp per metre (the maze with its walls is about 62 m: a bit wider than a phone, so
the map pans). It pans so the player's marker stays in the centre.

| Thing | Drawn as |
|---|---|
| Floor | faint sand fill over the whole map |
| Walls | sandstone outlines with a faint fill |
| Rails | teal, 0.8 m wide; straight runs are one box, curves a row of dots |
| Coins still out | small dots in the level's colour (yellow, green, blue, red, violet) |
| Hunters | dots in their colours (Blinky red, Pinky pink, Inky cyan, Clyde orange) |
| Player | white dot in the centre, with a small dot in front of it showing the way it faces |

## Models

| Model | When | What |
|---|---|---|
| `map` | once, at session start | `width`, `depth` (m), `walls` `[x0, z0, x1, z1]`, `rails` `[x0, z0, x1, z1, round]`, `coins` `[x, z]` |
| `map_coins` | on start, pickups, level change, new run | `level`, `out` (one character per coin: `1` = still there) |
| `map_live` | every other tick, only while the map is open | player `x`, `z`, heading `hx`, `hz`; `hunters` `[{name, x, z}]` |
| `run.map_open` | with the run model | the map is open |

Coordinates are metres from the map's north-west corner (x east, z south). Requests:
`map_open` (button pressed, only in play), `map_close` (let go).

## No engine change

The engine's UI has no line or path drawing, so the map is built from plain UI boxes: walls and
straight rails are rectangles, curves rows of dots. The maze is all straight corridors, so this
is enough. Engine request #7 (map drawing) isn't needed for now.

## Not yet (polish)

- The heading shows as a dot in front of the marker, not a rotated arrow: the UI can't rotate
  elements yet (`engine/ui` README "Limits"). Ask for rotation in the UI when the map gets its
  art.
- Opening / closing has no fade; the tuning of scale, colours and the backdrop's darkness waits
  for the phone playtest (milestone 7).
- Desktop has no key for the map: hold the button with the mouse.
