# Engine request #3 + #4: AI hunters on the rails (Coin Rush milestone 4)

Paste this into the Robotag: Game Engine project. It is everything Coin Rush's hunters need
from the engine, sized as one small milestone in `modules/robots`. The game side (who to chase,
which exit to take, release and scatter timers, speeds per level, what a catch does) is already
written in the game repo (`code/core/hunters.hpp`, tested), so the engine only needs to give
game code a steering wheel and two contact rules.

## Why

Coin Rush has four AI hunters (Pac-Man ghosts on Robotag rails). Today the only non-player
driver is `Robot.autopilot`, which takes random exits. Game code can read robots (Transform,
`RailRider`, `contact_log()`) but can't drive them, place them or stop them, and every contact
is a bounce. The `engine-api-for-games` robots page already lists these as "not yet".

## What the game needs

### A. Steering from game code (request #3)

1. **A game driver for a robot.** Game code registers a function for a robot entity; the
   Robots system calls it where autopilot would pick a random exit (each tick while the robot
   has no plan for its next intersection, and is still before that intersection's early line),
   and its answer becomes the robot's premove. Proposed API in `robots/core/robots_module.hpp`:

   ```cpp
   struct DriveRequest {
       std::string robot;   // Entity id.
       const RailMap* map;
       const Rider* rider;  // Read-only.
       int node;            // The next intersection ahead (through two-port nodes), or the
       int entry;           // node it is parked on; the port it arrives / arrived by.
   };
   // nullopt = no plan yet (ask again next tick). Swipe::Down is never taken from a driver
   // (hunters never reverse); a parked robot takes the answer as an order.
   using Driver = std::function<std::optional<Swipe>(const DriveRequest&)>;
   void set_driver(const std::string& robot_id, Driver driver);  // {} = back to the Robot's own setting.
   ```

   Plans from a driver are early plans (full speed through the junction), so a game-driven
   robot never stops, the same as autopilot. Cleared when a session starts.

2. **Speed from game code.** `void set_top_speed(const std::string& robot_id, float mps);`
   takes effect from the next tick (the hunters speed up on each level). A robot that is
   already moving eases to the new speed or just switches; either is fine.

3. **Hold a robot.** `void set_held(const std::string& robot_id, bool held);` ("" = every
   robot). A held robot stays where it is (parked, or brakes to a stop at once) and ignores
   swipes and its driver until released. Used for: hunters waiting at home until their release
   time, the 3-2-1 countdown, and "everything freezes" when the player is caught.

4. **Place a robot on a node.** `void place_robot(const std::string& robot_id, const std::string& node_id, int entry_port);`
   parks the robot there on the next tick (plan, brake and speed cleared). Used for the random
   player spawn at run start (already wanted by milestone 2) and Play Again.

All four apply on the next tick of the running session, like `trigger_robot_anim`, so a game
Instance (which runs after Robots) can call them from its tick.

### B. Contact rules (request #4)

Two fields on the `Robot` component (adding fields needs no migration):

```
contact_group: string = ""   // Robots in the same non-empty group never touch (pass through).
bounce:        bool   = true // false = contacts with this robot are logged, nobody bounces.
```

Coin Rush sets the four hunters to `contact_group: "hunters"`, `bounce: false`. Hunter vs
hunter: nothing happens. Hunter vs player: one `ContactEvent` in `contact_log()` (the game
reads it as the catch and holds every robot), no bounce, no push-through. Robots without the
fields behave exactly as today (sandbox unchanged). Logging the contact once per touch is
enough; the game ends the run on the first one.

## Checks

- `robots_test`: a driver that always says Left takes the left exit at every intersection at
  full speed (never parks); a driver's Down is ignored; `set_top_speed` changes the speed;
  a held robot doesn't move and resumes when released; `place_robot` parks on the node.
- Contacts: two robots in one group pass through each other; `bounce: false` logs one event
  and both keep their speed.
- `engine-api-for-games` robots page: move these out of "not yet"; the planned skill
  `drive-robots` can be a short section there instead.

## Not needed

Path finding (the game picks exits itself), multiplayer, network sync, anything visual.
