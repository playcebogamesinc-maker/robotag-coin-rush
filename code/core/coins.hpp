#pragma once

// Coins and levels (docs/gameplay.md "Coins, levels and scoring"): coins line every rail of the
// maze at even spacing, the player picks them up by driving over them, and clearing the board
// starts the next level, where every coin comes back in the same spot, in a new colour and
// worth twice as much. Clearing level 5 wins the run. No scene or graphics here: the run
// (coin_rush.cpp) puts the coins in the scene and feeds the player's position in.

#include <array>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "rails/core/rail_graph.hpp"

namespace coin_rush {

// Metres between coins along a rail (each rail gets a whole number of coins, as close to
// this as fits, the end ones half a gap from its nodes).
inline constexpr float kCoinSpacing = 3.0f;
// Coins float this high above the track (where the wheel rides: rails::ride_height).
inline constexpr float kCoinHeight = 0.5f;
// A coin is picked up when the player robot is this close to it (on the ground plane).
inline constexpr float kPickupRadius = 1.0f;

struct Level {
    int points;            // Per coin.
    const char* colour;    // As the design names it.
    const char* material;  // The coins' material (content/).
};

// The five levels of a run, in order.
inline constexpr std::array<Level, 5> kLevels = {{
    {1, "yellow", "materials/coin_yellow.material.json"},
    {2, "green", "materials/coin_green.material.json"},
    {4, "blue", "materials/coin_blue.material.json"},
    {8, "red", "materials/coin_red.material.json"},
    {16, "violet", "materials/coin_violet.material.json"},
}};

// Where a coin sits: `s` metres along rail `rail`, at `position` (world, above the rail).
struct CoinSpot {
    std::string rail;
    float s = 0.0f;
    glm::vec3 position{0.0f};
};

// Every coin of a maze: along each rail (not the corner rails inside junctions), in rail order.
std::vector<CoinSpot> coin_spots(const rails::RailGraph& graph, float ride_height = 0.0f,
                                 float spacing = kCoinSpacing);

// The coins of one run: which are still out, the level and the score.
class CoinBoard {
public:
    explicit CoinBoard(std::vector<CoinSpot> spots);

    struct Pickup {
        std::vector<int> taken;  // Coins picked up by this move (indices into spots()).
        int points = 0;          // What they were worth.
        bool level_cleared = false;  // The last coin of a level went: the next level started
                                     // (all coins back) or, after level 5, the run is won.
    };
    // The player robot is at `position`: picks up every coin in reach. Does nothing once won.
    Pickup collect(const glm::vec3& position);

    const std::vector<CoinSpot>& spots() const { return spots_; }
    bool out(int coin) const { return out_[static_cast<size_t>(coin)]; }
    int left() const { return left_; }
    int level() const { return level_; }  // 1..5.
    const Level& level_info() const { return kLevels[static_cast<size_t>(level_ - 1)]; }
    int score() const { return score_; }
    bool won() const { return won_; }

private:
    std::vector<CoinSpot> spots_;
    std::vector<bool> out_;
    int left_ = 0;
    int level_ = 1;
    int score_ = 0;
    bool won_ = false;
};

}  // namespace coin_rush
