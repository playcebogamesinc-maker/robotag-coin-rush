#pragma once

// The maze: the scene's railway plus the MazeSpots that name its places (hunter homes in the
// corners, player spawns near the centre), and the layout rules from docs/gameplay.md
// ("The arena") as checks. Read-only: the layout itself is content (tools/maze_layout.py).

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "rails/core/rail_graph.hpp"
#include "scene/scene.hpp"

namespace coin_rush {

// The hunters in home order (MazeSpot.role).
inline constexpr std::array<const char*, 4> kHunters = {"blinky", "pinky", "inky", "clyde"};

struct Maze {
    rails::RailGraph graph;
    float ride_height = 0.0f;          // Wheel bottom above the rails (rails::ride_height).
    std::vector<std::string> spawns;   // Player spawn nodes, in scene order.
    std::array<std::string, 4> homes;  // Home node per hunter (kHunters order), "" = missing.
};

// The maze of a running or loaded scene: its RailNetwork's railway (path relative to
// `content_root`, or as-is when that is empty) and its MazeSpots. nullopt + error when the
// scene has no railway or it doesn't load.
std::optional<Maze> load_maze(const scene::Scene& scene, const std::string& content_root,
                              std::string& error);

// What breaks the layout rules (empty = none): dead ends and stuck entries, corridors outside
// 10-26 m, a missing or duplicate home or one that isn't its quadrant's corner, spawns that
// aren't intersections within 15 m of the centre, a layout not mirrored left to right.
std::vector<std::string> check_maze(const Maze& maze);

// The spawn node for a run from a random seed ("" when the maze has none).
std::string pick_spawn(const Maze& maze, uint32_t seed);

}  // namespace coin_rush
