#pragma once

// The hold-to-view map (docs/gameplay.md "HUD and map", docs/map.md): what the full-screen map
// draws, as plain rectangles the game UI can show (walls, rails, the floor) plus the coin spots,
// all on the ground plane (x, z) in metres, measured from the map's top-left (north-west) corner.
// No scene or graphics here: the run (coin_rush.cpp) reads the walls from its scene and sends
// the layout to content/ui/map.tsx once, as the "map" model.

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "core/coins.hpp"
#include "rails/core/rail_graph.hpp"

namespace coin_rush {

// Rails are drawn this wide on the map (metres; the real rail is much thinner, this reads).
inline constexpr float kMapRailWidth = 0.8f;

// A rectangle on the ground: x0 < x1 (west to east), z0 < z1 (north to south).
struct MapRect {
    float x0 = 0.0f, z0 = 0.0f, x1 = 0.0f, z1 = 0.0f;
    bool round = false;  // A dot on a curve (drawn round), else a straight piece.
};

struct MapLayout {
    glm::vec2 origin{0.0f};  // World (x, z) of the map's top-left corner.
    float width = 0.0f;      // Map size in metres (x)...
    float depth = 0.0f;      // ...and (z).
    std::vector<MapRect> walls;
    std::vector<MapRect> rails;
    std::vector<glm::vec2> coins;  // Every coin spot (CoinBoard order), map coordinates.
};

// The map of a maze: `walls` are the wall boxes in world space (x, z extents; the outer walls
// set the map's size), the rails come from `graph` (straight runs merged into one rectangle,
// curves as a row of dots), `coins` are the board's spots.
MapLayout build_map(const rails::RailGraph& graph, const std::vector<MapRect>& walls,
                    const std::vector<CoinSpot>& coins);

// World (x, z) -> map coordinates.
inline glm::vec2 to_map(const MapLayout& map, const glm::vec3& world) {
    return glm::vec2(world.x, world.z) - map.origin;
}

// The "map" model: {"width", "depth", "walls": [[x0, z0, x1, z1], ...], "rails": [[x0, z0, x1,
// z1, round], ...], "coins": [[x, z], ...]} with centimetre precision.
std::string map_layout_json(const MapLayout& map);

}  // namespace coin_rush
