# The hunters

Milestone 4. The rules come from [gameplay.md](gameplay.md) ("Hunters"); this page is how the
code meets them and what still waits for the engine.

## In the maze

Four robots named `Blinky`, `Pinky`, `Inky` and `Clyde` in `content/scenes/main.scene.json`,
each parked on its home corner (the `MazeSpot` of the same role), tinted in its colour
(`materials/hunter_<name>.material.json` body, `home_<name>` accents). The run finds them by
name, so keep the names.

Their Robots have `contact_group: "hunters"` (they pass through each other) and
`bounce: false` (touching the player is logged, nobody bounces).

The run (`code/core/coin_rush.cpp`) drives them: at the start it holds each hunter at home
(`robots::set_held`), gives each its brain as the robot's driver (`robots::set_driver` with
`choose_swipe(hunter_target(...))`) and sets the level 1 speed (`robots::set_top_speed`). Every
tick it releases hunters on time and updates each one's target; on each level it raises their
speed. A touch from a hunter ends the run: the first player-hunter contact in the robots'
contact log sets `caught` in the `run` model, every robot is held, the score stays and the HUD
shows "Caught!". The results screen and Play Again come with milestone 5.

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
| Hunters ignore each other | `contact_group: "hunters"` on their Robots. |

All numbers live in `HunterRules` and `kHunterSpeed`, to tune in milestone 7. The test drives
each hunter with its brain on the real maze against a player waiting on a spawn: all four get
there without a single stop or reverse.

## Tuning

Everything is in `HunterRules` and `kHunterSpeed` (`code/core/hunters.hpp`); milestone 7 tunes
them on a phone. `coin_rush_test` checks the release order, that hunters never touch each
other, that a player standing still gets caught (by Blinky, about 20 s today) and that
everything stands still after the catch.
