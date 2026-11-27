# Engine request #6 (+ #5 part): save data and "app went to the background" (Coin Rush milestone 5)

Paste this into the Robotag: Game Engine project. It is small: one key-value store for game
code plus one event. Coin Rush's run flow (start screen, countdown, pause, results, Play Again)
is already done in the game repo without engine changes; these two things are the only parts it
can't do itself.

## Why

- Coin Rush shows the **best score** on the start screen and results ("New best!"). Today it can
  only keep it in memory, so it resets every time the app closes. The engine has no place for
  game code to write files on a phone (Android has no writable content folder; `content_root` is
  "" there), and `engine-api-for-games` has no save data API.
- A phone game must **pause when the app goes to the background** (a call, the home button). The
  engine already pauses audio in the background, but game code isn't told.

## What the game needs

### A. Save data (request #6)

A small persistent key-value store per project, read and written from game code (a system's
`start`, `tick` or `ui_request`). Proposed API in `runtime/save_data.hpp` (library
`rt_runtime`, no graphics):

```cpp
namespace runtime {
// Values saved for the open project, kept across app launches. Keys are short ASCII names.
std::optional<std::string> load_value(const std::string& key);   // nullopt = never saved.
void save_value(const std::string& key, const std::string& value);  // Written soon, atomically.
}
```

- Where: desktop = the project's `.cache/save/<game>.json` (or the user's app data folder);
  Android = the app's internal files folder (`SDL_GetPrefPath`). One JSON object per game.
- Writes are cheap to call from a tick: the store keeps the values in memory and writes the
  file on a worker thread (or at the end of the frame), atomically (write temp, rename), so a
  crash never leaves half a file.
- Tests and `rt render --play` get an in-memory store (nothing on disk), with a way for tests to
  preset values (e.g. `runtime::reset_save_data({{"best", "120"}})`).
- The editor's Play mode uses the real store, and a small "Clear save data" item somewhere
  (Window > Robots or the Game panel menu) would help testing.

Coin Rush would use it like this:

```cpp
const int best = std::stoi(runtime::load_value("best_score").value_or("0"));
runtime::save_value("best_score", std::to_string(score));
```

### B. App background / foreground (part of request #5, game flow)

`runtime::Instance` gets a call when the app goes to the background and comes back:

```cpp
virtual void app_paused(scene::Scene& scene, bool paused) {}  // true = backgrounded.
```

The player calls it (SDL `SDL_EVENT_WILL_ENTER_BACKGROUND` / `DID_ENTER_FOREGROUND`, and window
focus loss on desktop is optional); the session should also stop ticking while backgrounded, as
it probably does already. Coin Rush answers by opening its pause menu.

The rest of request #5 (pause, restart) is no longer needed: Coin Rush does both in game code
with `set_held` / `place_robot`. Scene switching isn't needed yet (one scene).

## Done when

- `engine-api-for-games/runtime.md` lists `load_value` / `save_value` and `app_paused` as
  stable, with an example, and the "Not yet" line for game flow / save data is gone.
- A test saves a value, restarts the store, and reads it back; the player keeps it across app
  launches on Android.

## Game side once it lands

`code/core/coin_rush.cpp`: read `best_score` at run start, save it in `end_run` when it's a new
best; pause on `app_paused(true)`. About ten lines plus a test.
