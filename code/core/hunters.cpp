#include "core/hunters.hpp"

#include <algorithm>

namespace coin_rush {

namespace {

constexpr std::array<const char*, 4> kNames = {"Blinky", "Pinky", "Inky", "Clyde"};

glm::vec2 flat(const glm::vec3& p) {
    return {p.x, p.z};
}

glm::vec3 node_position(const robots::RailMap& map, int node) {
    return map.nodes()[static_cast<size_t>(node)].node->data.position;
}

// From (node, entry) on through two-port nodes to the first intersection (or dead end).
Stop through_two_port(const robots::RailMap& map, Stop stop) {
    for (int guard = 0; guard < 256 && stop.node >= 0; ++guard) {
        if (!map.pass_through(stop.node, stop.entry)) return stop;
        const robots::RailMap::NodeInfo& n = map.nodes()[static_cast<size_t>(stop.node)];
        const int exit = n.turns[static_cast<size_t>(stop.entry)].front();
        const int t = n.port_track[static_cast<size_t>(exit)];
        if (t < 0) return {};
        const robots::RailMap::Track& track = map.tracks()[static_cast<size_t>(t)];
        const bool from_start = track.ends[0].node == stop.node && track.ends[0].port == exit;
        const robots::RailMap::End& far = track.ends[from_start ? 1 : 0];
        stop = {far.node, far.port};
    }
    return stop;  // A ring without intersections: give the last node.
}

}  // namespace

const char* hunter_name(Hunter hunter) {
    return kNames[static_cast<size_t>(hunter)];
}

std::optional<Hunter> hunter_from_name(const std::string& name) {
    for (size_t i = 0; i < kNames.size(); ++i) {
        if (name == kNames[i]) return static_cast<Hunter>(i);
    }
    return std::nullopt;
}

float hunter_speed(int level, float player_top_speed) {
    const int i = std::clamp(level, 1, static_cast<int>(kHunterSpeed.size())) - 1;
    return kHunterSpeed[static_cast<size_t>(i)] * player_top_speed;
}

const char* mode_name(HunterMode mode) {
    switch (mode) {
        case HunterMode::Home: return "home";
        case HunterMode::Scatter: return "scatter";
        case HunterMode::Chase: return "chase";
    }
    return "?";
}

HunterMode hunter_mode(Hunter hunter, int64_t ticks, const HunterRules& rules) {
    const int64_t release =
        rules.first_release_ticks + static_cast<int64_t>(hunter) * rules.release_gap_ticks;
    if (ticks < release) return HunterMode::Home;
    if (ticks < release + rules.scatter_ticks) return HunterMode::Scatter;
    return HunterMode::Chase;
}

glm::vec3 hunter_target(Hunter hunter, HunterMode mode, const Sighting& seen,
                        const HunterRules& rules) {
    if (mode != HunterMode::Chase) return seen.home;
    const glm::vec2 player = flat(seen.player);
    glm::vec2 forward = flat(seen.player_forward);
    const float len = glm::length(forward);
    forward = len > 1e-4f ? forward / len : glm::vec2(0.0f, -1.0f);
    const auto at = [&](glm::vec2 p) { return glm::vec3(p.x, seen.player.y, p.y); };

    if (glm::distance(flat(seen.self), player) < rules.close_range) return seen.player;
    switch (hunter) {
        case Hunter::Blinky:  // Chaser: right where you are.
            return seen.player;
        case Hunter::Pinky:  // Ambusher: ahead of you.
            return at(player + forward * rules.pinky_ahead);
        case Hunter::Inky: {  // Flanker: Blinky's vector to a point ahead of you, doubled.
            const glm::vec2 pivot = player + forward * rules.inky_ahead;
            return at(pivot * 2.0f - flat(seen.blinky));
        }
        case Hunter::Clyde:  // Wanderer: hunts from afar, goes home up close.
            return glm::distance(flat(seen.self), player) > rules.clyde_shy ? seen.player
                                                                           : seen.home;
    }
    return seen.player;
}

Stop next_stop(const robots::RailMap& map, int node, int port) {
    if (node < 0 || port < 0) return {};
    const robots::RailMap::NodeInfo& n = map.nodes()[static_cast<size_t>(node)];
    if (port >= static_cast<int>(n.port_track.size())) return {};
    const int t = n.port_track[static_cast<size_t>(port)];
    if (t < 0) return {};
    const robots::RailMap::Track& track = map.tracks()[static_cast<size_t>(t)];
    const bool from_start = track.ends[0].node == node && track.ends[0].port == port;
    const robots::RailMap::End& far = track.ends[from_start ? 1 : 0];
    return through_two_port(map, {far.node, far.port});
}

Stop decision_point(const robots::RailMap& map, const robots::Rider& rider) {
    if (rider.parked) return {rider.node, rider.entry};
    if (rider.track < 0) return {};
    const robots::RailMap::Track& track = map.tracks()[static_cast<size_t>(rider.track)];
    const robots::RailMap::End& end = track.ends[rider.dir > 0 ? 1 : 0];
    // A corner rail ends on the node it cuts across, at the port it leaves by.
    if (track.junction) return next_stop(map, end.node, end.port);
    return through_two_port(map, {end.node, end.port});
}

std::optional<robots::Swipe> choose_swipe(const robots::RailMap& map, int node, int entry,
                                          const glm::vec3& target) {
    static constexpr robots::Swipe kWays[] = {robots::Swipe::Up, robots::Swipe::Left,
                                              robots::Swipe::Right};
    std::optional<robots::Swipe> best;
    int best_exit = -1;
    float best_distance = 0.0f;
    for (robots::Swipe way : kWays) {
        const int exit = map.resolve(node, entry, way);
        if (exit < 0 || exit == entry || exit == best_exit) continue;
        const Stop stop = next_stop(map, node, exit);
        if (stop.node < 0) continue;
        const float d = glm::distance(flat(node_position(map, stop.node)), flat(target));
        if (!best || d < best_distance - 1e-3f) {
            best = way;
            best_exit = exit;
            best_distance = d;
        }
    }
    return best;
}

bool is_catch(const std::string& a, const std::string& b, const std::string& player) {
    return (a == player && hunter_from_name(b)) || (b == player && hunter_from_name(a));
}

}  // namespace coin_rush
