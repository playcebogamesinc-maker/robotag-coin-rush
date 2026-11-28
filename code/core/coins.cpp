#include "core/coins.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "rails/core/rail_bake.hpp"

namespace coin_rush {

std::vector<CoinSpot> coin_spots(const rails::RailGraph& graph, float ride_height, float spacing) {
    std::vector<CoinSpot> spots;
    rails::BakeCache bakes;
    bakes.update(graph);
    for (const rails::Edge& edge : graph.edges) {
        const rails::EdgeBake* bake = bakes.find(edge.id);
        if (!bake || bake->length <= 0.0f) continue;
        const int count = std::max(1, static_cast<int>(std::lround(bake->length / spacing)));
        const float gap = bake->length / static_cast<float>(count);
        for (int i = 0; i < count; ++i) {
            const float s = (static_cast<float>(i) + 0.5f) * gap;
            const rails::RailFrame f = bake->at(s);
            spots.push_back({edge.id, s, f.position + f.up * (ride_height + kCoinHeight)});
        }
    }
    return spots;
}

CoinBoard::CoinBoard(std::vector<CoinSpot> spots)
    : spots_(std::move(spots)), out_(spots_.size(), true), left_(static_cast<int>(spots_.size())) {}

CoinBoard::Pickup CoinBoard::collect(const glm::vec3& position) {
    Pickup pickup;
    if (won_ || spots_.empty()) return pickup;
    const glm::vec2 at(position.x, position.z);
    for (size_t i = 0; i < spots_.size(); ++i) {
        if (!out_[i]) continue;
        const glm::vec2 coin(spots_[i].position.x, spots_[i].position.z);
        if (glm::length(coin - at) > kPickupRadius) continue;
        out_[i] = false;
        --left_;
        pickup.taken.push_back(static_cast<int>(i));
        pickup.points += level_info().points;
    }
    score_ += pickup.points;
    if (left_ == 0) {
        pickup.level_cleared = true;
        if (level_ == static_cast<int>(kLevels.size())) {
            won_ = true;
        } else {
            ++level_;
            std::fill(out_.begin(), out_.end(), true);
            left_ = static_cast<int>(spots_.size());
        }
    }
    return pickup;
}

}  // namespace coin_rush
