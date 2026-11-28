#include "core/maze.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>

#include "rails/core/rail_bake.hpp"
#include "rails/core/rail_junctions.hpp"
#include "rails/core/rail_style.hpp"
#include "schema/coin_rush.gen.hpp"

namespace coin_rush {

namespace {

// docs/gameplay.md: corridors of ~12 / 18 / 24 m, with some slack for curves.
constexpr float kMinCorridor = 10.0f;
constexpr float kMaxCorridor = 26.0f;
constexpr float kSpawnRadius = 15.0f;  // Spawns: intersections this close to the centre.
constexpr float kMirrorTolerance = 0.25f;

template <class T>
const T* find_component(const scene::Entity& e) {
    for (const auto& box : e.module_components) {
        if (const T* c = scene::module_component_as<T>(*box)) return c;
    }
    return nullptr;
}

std::string fmt(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(v));
    return buf;
}

glm::vec2 flat(const rails::Node& n) {
    return {n.data.position.x, n.data.position.z};
}

// The middle of the layout's bounds (x, z).
glm::vec2 centre_of(const rails::RailGraph& graph) {
    glm::vec2 lo(1e9f), hi(-1e9f);
    for (const rails::Node& n : graph.nodes) {
        lo = glm::min(lo, flat(n));
        hi = glm::max(hi, flat(n));
    }
    return graph.nodes.empty() ? glm::vec2(0.0f) : (lo + hi) * 0.5f;
}

int quadrant(glm::vec2 p, glm::vec2 centre) {
    return (p.x >= centre.x ? 1 : 0) + (p.y >= centre.y ? 2 : 0);
}

}  // namespace

std::optional<Maze> load_maze(const scene::Scene& scene, const std::string& content_root,
                              std::string& error) {
    const rails::RailNetwork* network = nullptr;
    for (const scene::Entity& e : scene.entities) {
        if (!network) network = find_component<rails::RailNetwork>(e);
    }
    if (!network || network->rails.empty()) {
        error = "the scene has no RailNetwork with a rails file (the maze)";
        return std::nullopt;
    }
    const std::string path =
        content_root.empty() ? network->rails : content_root + "/" + network->rails;
    std::optional<rails::RailGraph> graph = rails::load_rails(path, error);
    if (!graph) return std::nullopt;

    Maze maze;
    maze.graph = std::move(*graph);
    maze.ride_height = rails::ride_height(*network);
    for (const scene::Entity& e : scene.entities) {
        const MazeSpot* spot = find_component<MazeSpot>(e);
        if (!spot) continue;
        if (spot->role == "spawn") {
            maze.spawns.push_back(spot->node);
            continue;
        }
        for (size_t i = 0; i < kHunters.size(); ++i) {
            if (spot->role == kHunters[i]) {
                if (!maze.homes[i].empty()) {
                    error = std::string("two homes for ") + kHunters[i] + " (entity \"" + e.name +
                            "\"); keep one MazeSpot per hunter";
                    return std::nullopt;
                }
                maze.homes[i] = spot->node;
            }
        }
    }
    return maze;
}

std::vector<std::string> check_maze(const Maze& maze) {
    std::vector<std::string> problems;
    const rails::RailGraph& graph = maze.graph;
    if (graph.nodes.empty()) return {"the railway has no nodes"};

    // Loops, not dead ends: every port has a rail and every way in has a way out.
    for (const rails::Node& node : graph.nodes) {
        if (node.ports.size() < 2) problems.push_back("node " + node.id + " is a dead end");
        const std::vector<std::vector<int>> turns = rails::node_turns(graph, node);
        for (size_t p = 0; p < node.ports.size(); ++p) {
            const std::string where = "node " + node.id + " port " + std::to_string(p);
            if (!graph.edge_at(node.id, static_cast<int>(p))) {
                problems.push_back(where + " has no rail (dead end)");
            } else if (p >= turns.size() || turns[p].empty()) {
                problems.push_back(where + ": arriving there has no way out");
            }
        }
    }

    // Corridor lengths.
    rails::BakeCache bakes;
    bakes.update(graph);
    for (const rails::Edge& edge : graph.edges) {
        const rails::EdgeBake* bake = bakes.find(edge.id);
        const float length = bake ? bake->length : 0.0f;
        if (length < kMinCorridor || length > kMaxCorridor) {
            problems.push_back("rail " + edge.id + " is " + fmt(length) + " m long (keep it " +
                               fmt(kMinCorridor) + "-" + fmt(kMaxCorridor) + " m)");
        }
    }

    // Four corners, four homes: one per quadrant, on the node farthest from the centre there.
    const glm::vec2 centre = centre_of(graph);
    std::array<const rails::Node*, 4> corner{};
    for (const rails::Node& n : graph.nodes) {
        const int q = quadrant(flat(n), centre);
        if (!corner[q] || glm::length(flat(n) - centre) > glm::length(flat(*corner[q]) - centre)) {
            corner[q] = &n;
        }
    }
    std::array<bool, 4> quadrant_used{};
    for (size_t i = 0; i < kHunters.size(); ++i) {
        const std::string who = std::string(kHunters[i]) + "'s home";
        const rails::Node* home = graph.find_node(maze.homes[i]);
        if (!home) {
            problems.push_back(who + (maze.homes[i].empty()
                                          ? " is missing (add a MazeSpot with that role)"
                                          : " names no rail node (\"" + maze.homes[i] + "\")"));
            continue;
        }
        const int q = quadrant(flat(*home), centre);
        if (quadrant_used[q]) problems.push_back(who + " shares a corner with another home");
        quadrant_used[q] = true;
        if (corner[q] != home) problems.push_back(who + " isn't its quadrant's corner node");
    }

    // Central start.
    if (maze.spawns.empty()) problems.push_back("no player spawn (add MazeSpots with role spawn)");
    for (const std::string& id : maze.spawns) {
        const rails::Node* n = graph.find_node(id);
        if (!n) {
            problems.push_back("spawn names no rail node (\"" + id + "\")");
        } else if (n->ports.size() < 3) {
            problems.push_back("spawn " + id + " isn't an intersection");
        } else if (glm::length(flat(*n) - centre) > kSpawnRadius) {
            problems.push_back("spawn " + id + " is " + fmt(glm::length(flat(*n) - centre)) +
                               " m from the centre (keep it within " + fmt(kSpawnRadius) + ")");
        }
    }

    // Readable shape: mirrored left to right.
    for (const rails::Node& n : graph.nodes) {
        const glm::vec2 mirror(2.0f * centre.x - n.data.position.x, n.data.position.z);
        const bool found = std::any_of(graph.nodes.begin(), graph.nodes.end(), [&](const auto& m) {
            return glm::length(flat(m) - mirror) < kMirrorTolerance &&
                   m.ports.size() == n.ports.size();
        });
        if (!found) problems.push_back("node " + n.id + " has no mirror image across the middle");
    }
    return problems;
}

std::string pick_spawn(const Maze& maze, uint32_t seed) {
    if (maze.spawns.empty()) return "";
    seed ^= seed >> 16;  // Mix so nearby seeds don't pick neighbours.
    seed *= 0x7feb352dU;
    seed ^= seed >> 15;
    return maze.spawns[seed % maze.spawns.size()];
}

}  // namespace coin_rush
