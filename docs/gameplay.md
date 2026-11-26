# RoboTag: Coin Rush — Gameplay Description

Nov 25, 2026 · Vlad

## Concept

Coin Rush is a single-player, fully offline Pac-Man on RoboTag's rails. You drive your robot through a dark labyrinth collecting every coin while four AI hunter robots try to catch you. One touch from a hunter ends the run.

The run lasts five levels. Each time you clear the board, the coins come back, they're worth double, and the hunters get faster. Clearing level 5 wins the run. The goal is the highest score, and getting all the way through is the ultimate brag.

It keeps everything that makes RoboTag feel like RoboTag: rail movement, swipe turns, premoves and the car-like braking and acceleration. What it adds is the classic Pac-Man tension of reading four chasers with different personalities and planning a route through the maze.

## Core loop

1. A 3-2-1 countdown plays. You spawn at a random intersection near the centre of the maze.
2. The hunters leave their homes one by one and start hunting.
3. You collect coins along the rails, weighing the shortest route against where the hunters are heading.
4. When the last coin is gone, the level is cleared. Every coin respawns in the same spot, in a new colour and worth twice as much, and the hunters speed up.
5. Repeat until you clear level 5 (a win) or a hunter touches you (a loss).
6. The results screen shows your final score and your best. Then you play again or go back to the menu.

Within each level, the moment-to-moment loop is: read the hunters, pick the next intersection, commit to the turn with a premove, grab the coins on that segment, repeat.

## Player and movement

The player drives a regular RoboTag robot, with their own skin and wheels, and controls it exactly as in multiplayer matches.

- **Rails only.** The robot always travels along rail segments between intersections and can never leave the track.
- **Swipe to steer.** A swipe picks the direction to take at the next intersection. A swipe back reverses on the current segment.
- **Premoves.** You can queue a turn before reaching an intersection. With a premove the robot flows through the junction without stopping, which is the key skill for fast, clean routes. A premove straight through an intersection keeps full speed.
- **No input means stopping.** Without a premove, the robot brakes and waits at the next intersection. Standing still is allowed but dangerous.
- **Car-like feel.** Braking bites, holds and lands softly. Acceleration is a hard launch that tapers into max speed. Both take about 0.5 s, so every stop costs time the hunters don't lose.
- **Premove indicator.** A rotating arrow on the HUD shows the turn you've queued.

## Coins, levels and scoring

Coins line every rail segment at even spacing, and a level is cleared when every coin is collected. There is no timer. Coins sit in the same positions on every level, so the maze becomes familiar and your route knowledge pays off.

| Level | Points per coin | Coin colour | Hunter speed |
|---|---|---|---|
| 1 | 1 | Yellow | Slowest, well below the player |
| 2 | 2 | Green | Faster |
| 3 | 4 | Blue | Faster |
| 4 | 8 | Red | Faster |
| 5 | 16 | Violet | Fastest, around player speed |

- Score is the only number shown during play. The level number isn't displayed; the coin colour tells you where you are.
- Doubling makes the later levels count for far more. One coin on level 5 is worth as much as 16 on level 1, so pushing deeper always beats playing safe.
- Getting caught ends the run, but you keep the score earned so far.
- Best score is saved and shown on the results screen, with a "New best!" callout when you beat it.

## Hunters

Four AI robots hunt the player. Each is a robot tinted in its own colour, and each has a classic Pac-Man personality. Together they produce chases that feel organised without being unfair.

| Hunter | Colour | Personality | What it aims for |
|---|---|---|---|
| Blinky | Red | Chaser | Your current position |
| Pinky | Pink | Ambusher | About 4 tiles ahead of you, in the direction you're heading |
| Inky | Cyan | Flanker | A point built from your position and Blinky's, so he closes in from the other side |
| Clyde | Orange | Wanderer | You when he's far away (beyond ~8 tiles); his home corner once he gets close |

Shared rules:

- **Decide only at intersections.** At each junction a hunter takes the exit that brings it closest to its target.
- **Never turn back.** A hunter never reverses mid-segment. This makes hunters readable and beatable: you can bait them past a turn.
- **Never stop.** Hunters keep rolling through junctions, while the player loses time on every stop. That difference is the main source of pressure.
- **Close-range override.** When a hunter is right by your intersection, it targets you directly. Standing still on a node is never safe.
- **Staggered release.** Each hunter starts in its own corner of the maze and leaves home one by one, a couple of seconds apart, so the opening seconds give you room to breathe.
- **Scatter, then chase.** After release, each hunter first heads for its own corner for a few seconds, then switches to hunting for the rest of the level.
- **Hunters ignore each other.** They pass through one another and don't block each other's paths.
- **Speed ramps per level.** All four speed up together each level (see the table above). Their behaviour and release timing stay the same.

There are no power pellets or frightened mode. You can never turn the tables, only outsmart them.

## The arena

The map is a single hand-built labyrinth of rails and walls. Its layout decides how the mode plays.

- **Loops, not dead ends.** Because hunters never reverse, loops let you shake them off. Dead ends are traps.
- **Four corners, four homes.** Each hunter's home is one of the maze's corners. That's where it spawns, where it scatters to, and where Clyde retreats.
- **Central start.** The player spawns at a random intersection near the centre, with clearance from coins and hunters.
- **Corridor length matters.** Segments that are too short give no time to decide a turn. Segments that are too long let hunters close the gap. The working baseline is mixed spacing: mostly medium corridors (~18 units), a few long arteries (~24) and short connectors (~12).
- **Readable shape.** The maze is mirrored left-to-right but not fully symmetric, so you can tell where you are on the map at a glance.
- **Run length.** One level should take roughly a minute to clear with good routing, so a full five-level run stays mobile-friendly.

## Run flow

1. **Enter** from the Events tab in the main menu. The event runs fully offline, with no matchmaking.
2. **Countdown.** A full-screen dark overlay shows 3-2-1 while everything is frozen.
3. **Level 1 starts.** The player can move, and the hunters begin their staggered release.
4. **Level clear.** The last coin is collected. The coins respawn in the next colour, the point value doubles, the hunters get faster, and play carries on.
5. **End of run.** It's either Caught!, when a hunter touches you on any level, or Cleared!, when you collect the last coin of level 5. Everything freezes.
6. **Results.** The screen shows the outcome, your final score, your best score (and "New best!" if you beat it), and Play Again and Back to Menu buttons.

Pausing freezes the player and the hunters. Closing the pause menu never revives a caught player.

## HUD and map

The HUD is kept minimal so the maze stays the focus.

- **Score.** The only running number on screen.
- **Premove arrow.** Shows your queued turn.
- **Hold-to-view map.** A button at the bottom middle opens a full-screen map over a dark, semi-transparent background, Diablo / Path of Exile style. It stays open only while you hold it, and your robot keeps moving but takes no input while it's open, so checking the map is a risk.
    - North-up. The map pans so your marker stays in the centre, and the marker rotates to show your heading.
    - It draws wall outlines in a warm sandstone tone and rails in teal, over a faint floor fill.
    - It shows the remaining coins as dots in the current level's colour, and the four hunters as dots in their own colours.
- **No corner minimap.** The full map replaces it.

## Look and feel

The setting is a dark, stylized Egyptian labyrinth.

- Warm sandstone walls, pillars and floor, built from simple shapes.
- Wall-mounted torches cast flickering pools of warm light. Deep shadows lie between them, and distant corridors fade into darkness.
- A bright, glowing rail cuts through the dark, so the route always reads clearly.
- Coins spin and bob in place, in the current level's colour.
- The hunters are robots with strong colour tints, recognisable at a glance even down a dark corridor.
- The polish target is Brawl Stars-level juice: satisfying coin pickups, a punchy level-clear moment, and a dramatic catch.

## Open questions

- [ ] Exact hunter speeds for level 1 and level 5, as a fraction of player speed. The working guess is ~0.75 rising to ~1.05.
- [ ] Do the coin colours clash with the hunters? Level 3 blue sits close to Inky's cyan, and level 4 red matches Blinky, especially as dots on the map.
- [ ] Should a level clear have its own celebration or a short breather before the next level?
- [ ] Is a five-level run too long for one life on mobile? If so, shrink the maze rather than speeding anything up.
- [ ] Final corridor spacing, once the current build has been played.
