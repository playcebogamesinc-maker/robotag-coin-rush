# The maze

Milestone 2. The rules come from [gameplay.md](gameplay.md) ("The arena"); this page is how the
current layout meets them and how to change it.

## Layout

54 x 54 m, north (-z) at the top. 16 rail nodes (12 intersections + 4 corners), 23 rails,
414 m of rail, low blue walls 2 m from every rail.

```
+-----+-----+-----+     one character = 3 m
|     |     |     |     + = rail node, - | = rail, blank = wall
|     |     |     |
|     |     |     |     corners: hunter homes (pink Pinky, red Blinky,
|     |     |     |              orange Clyde, cyan Inky)
|     |     |     |
|     |     |     |
|     |     |     |
+-----+-----+-----+     the four intersections around the
|     |     |     |     central block: player spawns
|     |     |     |
|     |     |     |
+-----+     +-----+
|     |     |     |
|     |     |     |
|     |     |     |
|     |     |     |
|     |     |     |
+-----+-----+-----+
```

| Rule | How |
|---|---|
| Loops, no dead ends | Every port has a rail; every way into a node has a way out. |
| Corridors 12 / 18 / 24 m | 15 rails of 18 m, 4 of 24 m (top band), 4 of 12 m (middle connectors). |
| Four corners, four homes | Two-port corner nodes; robots drive through them without stopping. The corner run between intersections is 42 m (24 + 18), the longest in the maze, near each hunter's home. |
| Central start | Four spawn intersections around the central block, 9-13 m from the centre. |
| Readable shape | Mirrored left to right; the bands are 24, 12 and 18 m tall, so top and bottom differ. |
| About a minute per level | Driving every rail from the start without a single premove (stopping at every intersection) takes 69 s in the test; premoves make it faster. |

Places are `MazeSpot` components (`schemas/coin_rush.schema`) on entities named `Home Blinky`
... `Spawn 4`: a role (`blinky`, `pinky`, `inky`, `clyde`, `spawn`) and the rail node they're
on. Their flat pads show them in renders and the editor. Game code reads the maze with
`load_maze()` / `check_maze()` (`code/core/maze.hpp`); `coin_rush_test` fails when a rule
breaks, and a run prints any broken rule at start.

Not yet: a run picking a random spawn. Today the player starts on `Spawn 3`; the engine can now place
a robot on a node (`robots::place_robot`); the run flow (milestone 5) picks the spawn.

## Change it

Edit `LAYOUT` (and `HOMES`, `SPAWNS`, `START`) in `tools/maze_layout.py`, then from the engine
root (Windows: `rt.cmd`, `python`):

```
python3 games/coin-rush/tools/maze_layout.py games/coin-rush/content/scenes/main.scene.json /tmp
echo '{"type": "rails", "version": 1}' > games/coin-rush/content/scenes/main.rails.json
build/linux/bin/Debug/rt apply games/coin-rush/content/scenes/main.rails.json /tmp/rails.json
build/linux/bin/Debug/rt apply games/coin-rush/content/scenes/main.scene.json /tmp/scene.json
build/linux/bin/Debug/rt validate games/coin-rush
ctest --test-dir build/linux -C Debug -L game:coin_rush --output-on-failure
build/linux/bin/Debug/rt render games/coin-rush/content/scenes/main.scene.json maze.png --game --size 800x800
```

The script rewrites the walls and spots it made before (ids `e_3a2e...`) and leaves the rest
of the scene alone. Nodes keep their ids only while the layout keeps its node order.
