# Robotag: Coin Rush

A single-player, fully offline Pac-Man on RoboTag's rails, built on the Robotag game engine.
Collect every coin in a dark labyrinth while four AI hunter robots try to catch you. Five
levels; the coins double in value and the hunters get faster each level.

## Setup

This repo lives inside a checkout of the engine (`robotag-game-engine`):

```
cd <engine checkout>
git clone https://github.com/playcebogamesinc-maker/robotag-coin-rush games/coin-rush
build.cmd
```

The engine build finds `games/coin-rush/code` by itself. Open "Coin Rush" in the Robotag Hub,
press Play in the editor (Ctrl+P), or build an APK from the editor's Build panel.

Design: [docs/gameplay.md](docs/gameplay.md). Rules for agents and the milestone status:
[CLAUDE.md](CLAUDE.md).
