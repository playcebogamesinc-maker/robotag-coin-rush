# CLAUDE.md

Coin Rush is a Robotag game (game id `coin_rush`): a single-player, offline Pac-Man on
RoboTag's rails. It is its own product in its own repo (`robotag-coin-rush`), cloned into the
engine checkout at `games/coin-rush/`, so the engine's rules and skills apply: read
`../../AGENTS.md` and use the skills in `../../.claude/skills/`. Start with
`engine-api-for-games` (what game code may use) and `write-game-code` (code, schemas, tests),
then `build-and-test`, `edit-content`, `rail-layout`, `add-ui-screen`.

## Rules

- **Never change the engine from here.** Everything Coin Rush needs lives in this repo. If the
  engine is missing something (the skill marks it "not yet"), write an engine request in
  `docs/engine-requests/` and tell Vlad; he builds it in the engine project.
- **Milestones.** Gameplay first, with simple meshes and no environment art. Polish only after
  the gameplay is confirmed. Status below.
- The design: `docs/gameplay.md`; the maze: `docs/maze.md`. Units are metres.

## Layout

- `coin_rush.rtproject`: name, start scene, `"game": "coin_rush"`.
- `content/`: scenes, materials, robots, settings, `ui/` (paths in documents are relative to it).
- `code/`: the game's C++ (`rt_add_game(coin_rush ...)`), built with the engine; tests run in
  ctest (`-L game:coin_rush`).
- `code/core/coins.hpp`: coins along the rails, pickup, levels and score (`docs/coins.md`).
- `code/core/coin_rush.cpp`: the `Run` system: run flow (menu, countdown, play, pause, results), the `run` UI model and requests (`docs/run-flow.md`).
- `code/core/hunters.hpp`: the hunters' brains: targets, exit choice, release, scatter / chase,
  speed per level, catches (`docs/hunters.md`).
- `schemas/coin_rush.schema`: the game's components (`MazeSpot`: hunter homes, player spawns).
- `code/core/map.hpp`: the hold-to-view map's drawing (walls, rails, coins as boxes; `docs/map.md`).
- `tools/maze_layout.py`: the maze layout as ASCII -> rt commands (`docs/maze.md`).
- `tools/environment.py`: the level look (Egyptian tunnels from basic shapes) -> `content/models/env/*.glb`, materials + rt commands (`docs/environment.md`).
- `tools/textures.py`: the stone textures + normal maps -> `content/textures/env/*.png`.

## Milestones

| # | Milestone | State |
|---|---|---|
| 1 | Project setup: project, test track, player robot, first system (`run` model -> score HUD), test | done |
| 2 | The maze: the real rail labyrinth (loops, 12 / 18 / 24 m corridors, corner homes, central spawns) | done: `docs/maze.md`, `tools/maze_layout.py`, `MazeSpot`, `code/core/maze.hpp` (random spawn: `robots::place_robot` exists now, wired with the run flow in M5) |
| 3 | Coins and score: coins along rails, pickup, 5 levels with doubling value and colours | done: `docs/coins.md`, `code/core/coins.hpp` (`coin_spots`, `CoinBoard`), coin materials; win after level 5 is only a flag until the run flow (M5) |
| 4 | Hunters: Blinky, Pinky, Inky, Clyde | done: `docs/hunters.md`, brains in `code/core/hunters.hpp`, driven with the engine's `set_driver` / `set_held` / `set_top_speed`, contact group "hunters", a catch ends the run |
| 5 | Run flow and HUD: start screen, countdown, pause, results, best score, premove arrow | done: `docs/run-flow.md`, `Run` phases in `code/core/coin_rush.cpp`, screens in `content/ui/` (`hud.tsx`, `screens.tsx`), random spawn with `place_robot`; best score saved across launches waits for engine request #6 (`docs/engine-requests/save-data.md`) |
| 6 | Hold-to-view map | done: `docs/map.md`, `code/core/map.hpp` (walls, rails, coins as boxes), `content/ui/map.tsx`, MAP button in `hud.tsx`; the player takes no input while it's open (a silent driver); no engine change |
| 6b | Level look: Egyptian pyramid tunnels from basic shapes (static environment only) | done: `docs/environment.md`, `tools/environment.py`, `tools/textures.py`; stone textures, 128 baked torch lamps, fog, corner darkening; torch flicker waits for engine request #10 B |
| 7 | Playtest and tune on a phone | |

## Checks (from the engine root, `cd ../..`)

```
cmake --build --preset linux-debug            # Windows: build.cmd
ctest --test-dir build/linux -C Debug -L game:coin_rush --output-on-failure
build/linux/bin/Debug/rt validate games/coin-rush
build/linux/bin/Debug/rt ui build games/coin-rush --check
build/linux/bin/Debug/rt render games/coin-rush/content/scenes/main.scene.json out.png \
    --game --ui --size 540x1170 --play 2
```

## Pull requests and CI

Same flow as the engine (Vlad, Nov 26 2026): work goes to a branch and a PR into `main`; agents
merge their own PRs once CI is green.

- CI: `.github/workflows/build.yml` (from the Hub's template) builds the engine with this game
  in `games/coin-rush/`, runs `ctest -L game:coin_rush`, the UI check and `rt validate`. The
  check is `windows`.
- Where it runs: repository variable `CI_RUNNER` (`github` = GitHub's cloud, free for this
  public repo; `local` / unset = this repo's self-hosted runner on Vlad's PC, set up on the
  Hub's Builds page). Switch it on the Hub's Builds page.
- The workflow needs the repository secret `ENGINE_TOKEN` (fine-grained token, Contents:
  Read-only on `robotag-game-engine`).
- Build and run the checks above locally first, push, open the PR, subscribe to its activity
  and stop (don't poll CI). Fix a CI failure on the same branch. Once the `windows` check is
  green, merge with a merge commit (auto-merge if the repo allows it:
  `gh pr merge <n> --auto --merge`), and tell Vlad.
