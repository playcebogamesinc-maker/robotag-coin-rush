# Run flow and HUD (milestone 5)

How a Coin Rush run starts, pauses and ends. Design: `gameplay.md` ("Core loop", "Run flow",
"HUD and map"). Code: the `Run` system in `code/core/coin_rush.cpp`; screens in
`content/ui/` (`main.tsx`, `hud.tsx`, `screens.tsx`).

## Phases

| Phase | What you see | Robots | Leaves by |
|---|---|---|---|
| `menu` | Start screen: title, one-line rules, best score (once there is one), **Play** | all held | Play |
| `countdown` | Dark overlay with 3, 2, 1 (one second each) | all held; the player is already on its spawn | after 3 s -> `playing` |
| `playing` | HUD: score, pause button (top right), premove arrow (bottom) | player drives, hunters released on their timers | Pause, a catch, the last coin of level 5 |
| `paused` | Pause menu: **Resume**, **Restart**, **Menu**; coins stop turning | all held | Resume (player and released hunters go again), Restart (= Play), Menu |
| `over` | Results: **Caught!** (by who) or **Cleared!**, score, best or **New best!**, **Play Again**, **Menu** | all held (Cleared! plays the victory pose) | Play Again, Menu |

Play / Play Again / Restart all start a fresh run: board back to level 1 (yellow, 1 point),
score 0, hunters parked at home with their release clock at zero, the player parked on a random
spawn (one of the maze's central `MazeSpot` spawns, facing a random way), then the countdown.
A caught player is never revived: Over only leaves through a new run or the menu.

Desktop shortcuts (editor Play, `robotag_player`): **Space** / **Enter** = Play (Again),
**Escape** / **P** = pause / resume.

## Random spawn

`pick_spawn(maze, seed)` (`maze.hpp`) picks the node; the entry port (which way the robot
faces) comes from the same seed. The seed is the number of ticks since the session started
plus the run count, so it changes from run to run (it depends on how long you sat on a screen)
but stays deterministic for the same inputs (no wall clock, `engine-api-for-games` rules). The
robot is placed with the engine's `robots::place_robot` when the countdown starts, so you see
where you are during 3-2-1.

## The "run" model

The UI reads one model, `run` (C++ -> TSX), and sends four requests back.

| Field | Meaning |
|---|---|
| `score`, `level`, `coins_left`, `won`, `caught`, `caught_by`, `hunters` | as in milestones 3-4 |
| `phase` | `menu`, `countdown`, `playing`, `paused`, `over` |
| `countdown` | 3, 2, 1 in the countdown, else 0 |
| `best`, `new_best` | best score so far; this run beat it |
| `plan` | the queued premove (`up`, `left`, `right`, `down`, or empty): the HUD arrow |
| `spawn` | the node this run started on |
| `map_open` | the map button is held (milestone 6, [map.md](map.md)) |

Requests: `play` (menu, results, pause menu), `pause` (playing), `resume` (paused), `menu`
(results, pause menu), `map_open` / `map_close` (the map button, [map.md](map.md)). Anything else, or a request in the wrong phase, is ignored.

## Not yet

- **Best score is not saved on the phone.** It is kept while the app runs (Play Again, the
  editor's Play / Stop), and resets when the app closes. Saving it needs the engine's save data:
  engine request #6, `engine-requests/save-data.md`.
- **Auto-pause when the app goes to the background** needs a signal from the engine (same
  request, part B).
- "Back to Menu" goes to the start screen; the real main menu with the Events tab belongs to the
  full Robotag app later.
- The premove arrow is an arrow glyph that changes direction; the rotating animation is polish.
