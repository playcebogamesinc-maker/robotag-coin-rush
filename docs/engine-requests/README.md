# Engine requests

What Coin Rush needs from the engine that it doesn't have yet. Vlad builds these in the engine
project; this repo never changes the engine.

| # | Request | State |
|---|---|---|
| 1 | Game projects with their own code ([doc](https://claude.ai/artifact/BgjCQhwcuv5mG6Ft4dgL1D)) | done: engine M18 |
| 2 | Skills for game projects ([doc](https://claude.ai/artifact/A6WEKxUtrB28FsrL6pkvMd)) | done: `engine-api-for-games`, `write-game-code` |
| 3 | Robot steering for AI (exit choice, speed, never stop / never reverse), and placing a robot on a node from game code (random player spawn at run start, respawn on Play Again) | done (engine #62, Nov 26 2026): `set_driver`, `set_top_speed`, `set_held`, `place_robot` ([doc](hunters.md)) |
| 4 | Contact rules (hunters pass through each other, a catch is an event, not a bounce) | done (engine #62, Nov 26 2026): `Robot.contact_group`, `Robot.bounce` |
| 5 | Game flow: pause, restart, scene switching | pause and restart done in game code (milestone 5); still wanted: "app went to the background" so the game can pause ([doc](save-data.md), part B); scene switching not needed yet |
| 6 | Save data (best score across app launches) | requested Nov 27 2026 ([doc](save-data.md)); until then the best score lasts while the app runs |
| 7 | Map drawing in the UI (lines / paths, or a top-down view) | not needed: milestone 6 draws the map with plain UI boxes ([map.md](../map.md)); rotating UI elements (a heading arrow) would be polish later |
| 8 | Hub: a GitHub token per repo (engine, each game), falling back to one shared token | wanted by Vlad (Nov 26 2026), not blocking |
| 9 | Hub's game CI template (`tools/hub/templates/game-build.yml`): build from a short path; a game runner's work folder overflows Windows' 260-character paths (Coin Rush works around it with `subst R:`) | found Nov 26 2026, Coin Rush has the workaround |
| 10 | Distance fog on `Look` (far corridors fade into darkness), plus out-of-step torch flicker (nice to have) | fog done (engine M20, `Look.fog*`; used by `tools/environment.py` since Nov 28 2026); flicker still open ([doc](fog.md)) |
| 11 | Grooved rail profile: raised lips with a wheel groove, cut open at T / X / corner junctions so the groove stays continuous (general rails-module feature) | requested Nov 28 2026 ([doc](rail-profile.md)); Coin Rush keeps the tube until it lands |
