#pragma once

// The four hunters (docs/gameplay.md "Hunters"): who each one aims for, which exit it takes at
// an intersection, when it leaves home, when it stops scattering and starts hunting, how fast
// it drives on each level, and what counts as a catch. Pure logic, no scene: the run
// (coin_rush.cpp) feeds positions in and drives the hunter robots with the answers.
//
// Shared rules this code keeps:
//   - Decide only at intersections: choose_swipe() picks the exit whose next intersection is
//     closest (straight line) to the target, like the Pac-Man ghosts.
//   - Never turn back: choose_swipe() never answers Down.
//   - Never stop: a hunter always has a plan for its next intersection.
//   - Close-range override: near the player, every hunter targets the player.
//   - Staggered release, then scatter to its own corner, then chase for the rest of the run.
//   - Speed ramps per level; behaviour and release timing stay the same.


#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "robots/core/robot_motion.hpp"

namespace coin_rush {

// kHunters order (maze.hpp).
enum class Hunter { Blinky, Pinky, Inky, Clyde };
const char* hunter_name(Hunter hunter);  // "Blinky", ... (the robot entity's name).
std::optional<Hunter> hunter_from_name(const std::string& name);

// A "tile" of the design's distances, in metres (the layout grid, docs/maze.md).
inline constexpr float kTile = 3.0f;

struct HunterRules {
    int first_release_ticks = 60;  // Blinky leaves home 1 s after the run starts...
    int release_gap_ticks = 120;   // ...and the others follow 2 s apart.
    int scatter_ticks = 300;       // After release: 5 s heading for its own corner.
    float close_range = 2 * kTile;  // Closer than this to the player: target the player.
    float pinky_ahead = 4 * kTile;  // Pinky aims this far ahead of the player.
    float inky_ahead = 2 * kTile;   // Inky's pivot: this far ahead of the player.
    float clyde_shy = 8 * kTile;    // Clyde hunts beyond this, else goes home.
};

// Hunter speed per level as a share of the player's top speed (level 1 well below, level 5
// around the player's speed).
inline constexpr std::array<float, 5> kHunterSpeed = {0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
float hunter_speed(int level, float player_top_speed);  // m/s; level 1..5 (clamped).

enum class HunterMode { Home, Scatter, Chase };
const char* mode_name(HunterMode mode);
// The mode of hunter `hunter`, `ticks` after the run started (ticks keep counting across
// levels: the release happens once per run).
HunterMode hunter_mode(Hunter hunter, int64_t ticks, const HunterRules& rules = {});

// What the hunters see (world positions; y is ignored).
struct Sighting {
    glm::vec3 player{0.0f};
    glm::vec3 player_forward{0.0f, 0.0f, -1.0f};  // The way the player robot faces.
    glm::vec3 blinky{0.0f};
    glm::vec3 self{0.0f};
    glm::vec3 home{0.0f};  // Its home corner.
};
// Where hunter `hunter` aims in `mode` (Home: its home).
glm::vec3 hunter_target(Hunter hunter, HunterMode mode, const Sighting& seen,
                        const HunterRules& rules = {});

// The next intersection along the rails leaving `node` by `port` (through two-port nodes), and
// the port it is reached by. node -1 = no rail there.
struct Stop {
    int node = -1;
    int entry = -1;
};
Stop next_stop(const robots::RailMap& map, int node, int port);
// Where a rider decides next: the node it is parked on (and its entry), or the next
// intersection ahead of it.
Stop decision_point(const robots::RailMap& map, const robots::Rider& rider);

// The swipe that takes a hunter arriving at `node` by `entry` closest to `target`: among the
// exits up / left / right (never back), the one whose next intersection is nearest the target
// in a straight line; ties go up, then left, then right. nullopt = no exit.
std::optional<robots::Swipe> choose_swipe(const robots::RailMap& map, int node, int entry,
                                          const glm::vec3& target);

// A contact between robots `a` and `b` (entity names) is a catch: one is the player, the other
// a hunter.
bool is_catch(const std::string& a, const std::string& b, const std::string& player);

}  // namespace coin_rush
