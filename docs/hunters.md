# The hunters

Milestone 4. The rules come from [gameplay.md](gameplay.md) ("Hunters"); this page is how the
code meets them and what still waits for the engine.

## In the maze

Four robots named `Blinky`, `Pinky`, `Inky` and `Clyde` in `content/scenes/main.scene.json`,
each parked on its home corner (the `MazeSpot` of the same role), tinted in its colour
(`materials/hunter_<name>.material.json` body, `home_<name>` accents). The run finds them by
name, so keep the names.

A touch from a hunter ends the run (`code/core/coin_rush.cpp`): the run reads the robots'
contact log, and the first contact between the player and a hunter sets `caught` in the `run`
model. The score stays, no more coins are counted, and the HUD shows "Caught!". The results
screen and Play Again come with milestone 5.

## The brains

`code/core/hunters.hpp`, pure logic with tests in `coin_rush_test`:

| Rule | Code |
|---|---|
| Personalities | `hunter_target()`: Blinky = the player; Pinky = 4 tiles (12 m) ahead of the player; Inky = the point 2 tiles ahead of the player, mirrored away from Blinky; Clyde = the player beyond 8 tiles (24 m), his home corner closer in. |
| Decide only at intersections | `choose_swipe()`: of the exits up / left / right, the one whose next intersection is closest to the target in a straight line. |
| Never turn back | `choose_swipe()` never answers down. |
| Never stop | The hunter plans its next intersection as soon as it leaves the last one, so it always turns early, at full speed. |
| Close-range override | Within 2 tiles (6 m) of the player every hunter targets the player. |
| Staggered release | `hunter_mode()`: Blinky leaves 1 s after the start, the others 2 s apart. |
| Scatter, then chase | 5 s heading for its own corner after release, then chasing for the rest of the run. |
| Speed per level | `hunter_speed()`: 60 / 70 / 80 / 90 / 100 % of the player's top speed. |
| Hunters ignore each other | Engine request #4 (below). |

All numbers live in `HunterRules` and `kHunterSpeed`, to tune in milestone 7. The test drives
each hunter with its brain on the real maze against a player waiting on a spawn: all four get
there without a single stop or reverse.

## Waiting for the engine

Game code can't steer a robot yet, so in play the hunters drive on the engine's autopilot:
random exits, never stopping, all leaving home at once, at the level 1 speed (5.9 m/s) on
every level, and they bounce off each other and the player. Engine requests #3 and #4
([hunters.md](engine-requests/hunters.md)) add what's missing: a game driver per robot, speed,
hold and place from game code, and contact rules (hunters pass through each other; touching
the player is an event, not a bounce). Once they land, the run plugs the brains in:
`set_driver` with `choose_swipe(hunter_target(...))`, `set_held` until each release and after a
catch, `set_top_speed(hunter_speed(level))` on each level.
