# Coins and score

Milestone 3. The rules come from [gameplay.md](gameplay.md) ("Coins, levels and scoring"); this
page is how the game does them and where to change them.

## How it plays

- **Coins line every rail.** Each of the maze's rails gets a whole number of coins about 3 m
  apart (18 m rail = 6 coins, 24 m = 8, 12 m = 4), the end ones half a gap from the nodes, so
  intersections, corners and spawns stay clear. 138 coins in the current maze.
- **Pickup.** Driving within 1 m of a coin (on the ground plane) picks it up. Corners and
  junctions don't miss any: the coins nearest a node are 1.5 m out.
- **Levels.** When the last coin goes, every coin comes back in the same spot in the next
  colour, worth double. Clearing level 5 wins the run (`won` in the model); after that nothing
  more is picked up.

| Level | Points per coin | Colour | Material |
|---|---|---|---|
| 1 | 1 | Yellow | `materials/coin_yellow.material.json` |
| 2 | 2 | Green | `materials/coin_green.material.json` |
| 3 | 4 | Blue | `materials/coin_blue.material.json` |
| 4 | 8 | Red | `materials/coin_red.material.json` |
| 5 | 16 | Violet | `materials/coin_violet.material.json` |

A perfect run is worth 31 x 138 = 4,278 points.

- **HUD.** Only the score is shown. The game publishes the UI model `run`:
  `{"score", "level", "coins_left", "won"}`; level and coins left are there for later screens
  (results, the map) and for tests, not for the HUD.

## Where it lives

- `code/core/coins.hpp/.cpp`: `coin_spots()` (where coins go) and `CoinBoard` (pickup, levels,
  score). No scene or graphics, so tests drive it directly.
- `code/core/coin_rush.cpp`: the run puts one `Coin N` entity per spot into the running scene
  (a flattened built-in sphere, 0.6 m across, 0.6 m above the rail, turning with `Spin`),
  hides a coin's mesh when it's taken and shows them all again in the next level's material.
  Coins exist only while playing; they aren't in the scene file.
- Tuning: `kCoinSpacing`, `kCoinHeight`, `kPickupRadius` and the `kLevels` table in
  `coins.hpp`. Coin colours: the five materials (emissive, so they glow in the dark maze).
- Tests (`coin_rush_test`): coins on every rail at even spacing and off the nodes; levels double
  and level 5 wins; a coin counts once; a real drive from the spawn picks coins up and the score
  matches the coins gone.

Later (polish, after the gameplay is confirmed): a pickup effect and sound, a real coin model.
