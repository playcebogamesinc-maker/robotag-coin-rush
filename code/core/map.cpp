#include "core/map.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "rails/core/rail_bake.hpp"

namespace coin_rush {

namespace {

constexpr float kSampleStep = 0.5f;  // Metres between rail samples.

MapRect around(glm::vec2 a, glm::vec2 b, float half, bool round) {
    return {std::min(a.x, b.x) - half, std::min(a.y, b.y) - half, std::max(a.x, b.x) + half,
            std::max(a.y, b.y) + half, round};
}

// One rail as rectangles: runs of samples along x or z become one rectangle, the rest dots.
void add_rail(const rails::EdgeBake& bake, std::vector<MapRect>& out) {
    const int steps = std::max(1, static_cast<int>(std::ceil(bake.length / kSampleStep)));
    std::vector<glm::vec2> points;
    for (int i = 0; i <= steps; ++i) {
        const float s = bake.length * static_cast<float>(i) / static_cast<float>(steps);
        const glm::vec3 p = bake.at(s).position;
        points.emplace_back(p.x, p.z);
    }
    const float half = kMapRailWidth * 0.5f;
    const auto axis = [](glm::vec2 a, glm::vec2 b) {  // 0 = along x, 1 = along z, -1 = neither.
        const glm::vec2 d = b - a;
        const float len = glm::length(d);
        if (len < 1e-4f) return -1;
        if (std::abs(d.y) < 0.02f * len) return 0;
        if (std::abs(d.x) < 0.02f * len) return 1;
        return -1;
    };
    size_t i = 0;
    while (i + 1 < points.size()) {
        const int a = axis(points[i], points[i + 1]);
        if (a < 0) {  // A curve: a dot per sample.
            out.push_back(around(points[i], points[i], half, true));
            ++i;
            continue;
        }
        size_t j = i + 1;
        while (j + 1 < points.size() && axis(points[j], points[j + 1]) == a) ++j;
        out.push_back(around(points[i], points[j], half, false));
        i = j;
    }
    out.push_back(around(points.back(), points.back(), half, true));
}

void shift(MapRect& r, glm::vec2 by) {
    r.x0 -= by.x;
    r.x1 -= by.x;
    r.z0 -= by.y;
    r.z1 -= by.y;
}

void add_number(std::string& out, float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    std::string s = buf;  // Trim "12.50" -> "12.5", "3.00" -> "3".
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s == "-0") s = "0";
    out += s;
}

void add_rect(std::string& out, const MapRect& r, bool with_round) {
    out += '[';
    add_number(out, r.x0);
    out += ',';
    add_number(out, r.z0);
    out += ',';
    add_number(out, r.x1);
    out += ',';
    add_number(out, r.z1);
    if (with_round) out += r.round ? ",1" : ",0";
    out += ']';
}

}  // namespace

MapLayout build_map(const rails::RailGraph& graph, const std::vector<MapRect>& walls,
                    const std::vector<CoinSpot>& coins) {
    MapLayout map;
    map.walls = walls;
    rails::BakeCache bakes;
    bakes.update(graph);
    for (const rails::Edge& edge : graph.edges) {
        const rails::EdgeBake* bake = bakes.find(edge.id);
        if (bake && bake->length > 0.0f) add_rail(*bake, map.rails);
    }
    for (const CoinSpot& c : coins) map.coins.emplace_back(c.position.x, c.position.z);

    // The map's extent: everything drawn, plus a small margin.
    float x0 = std::numeric_limits<float>::max(), z0 = x0;
    float x1 = -x0, z1 = -x0;
    const auto grow = [&](const MapRect& r) {
        x0 = std::min(x0, r.x0);
        z0 = std::min(z0, r.z0);
        x1 = std::max(x1, r.x1);
        z1 = std::max(z1, r.z1);
    };
    for (const MapRect& r : map.walls) grow(r);
    for (const MapRect& r : map.rails) grow(r);
    if (x0 > x1) return map;  // Nothing to draw.
    constexpr float kMargin = 1.0f;
    map.origin = {x0 - kMargin, z0 - kMargin};
    map.width = x1 - x0 + 2 * kMargin;
    map.depth = z1 - z0 + 2 * kMargin;
    for (MapRect& r : map.walls) shift(r, map.origin);
    for (MapRect& r : map.rails) shift(r, map.origin);
    for (glm::vec2& c : map.coins) c -= map.origin;
    return map;
}

std::string map_layout_json(const MapLayout& map) {
    std::string out = "{\"width\":";
    add_number(out, map.width);
    out += ",\"depth\":";
    add_number(out, map.depth);
    out += ",\"walls\":[";
    for (size_t i = 0; i < map.walls.size(); ++i) {
        if (i) out += ',';
        add_rect(out, map.walls[i], false);
    }
    out += "],\"rails\":[";
    for (size_t i = 0; i < map.rails.size(); ++i) {
        if (i) out += ',';
        add_rect(out, map.rails[i], true);
    }
    out += "],\"coins\":[";
    for (size_t i = 0; i < map.coins.size(); ++i) {
        if (i) out += ',';
        out += '[';
        add_number(out, map.coins[i].x);
        out += ',';
        add_number(out, map.coins[i].y);
        out += ']';
    }
    out += "]}";
    return out;
}

}  // namespace coin_rush
