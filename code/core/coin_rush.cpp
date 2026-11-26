#include "core/coin_rush.hpp"

#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/maze.hpp"
#include "runtime/session.hpp"
#include "scene/module_component.hpp"
#include "schema/coin_rush.gen.hpp"

namespace coin_rush {

namespace {

// One run of Coin Rush: the maze (milestone 2) and the score the HUD shows. Coins, levels and
// hunters add to this run in later milestones.
class Run : public runtime::Instance {
public:
    explicit Run(std::optional<Maze> maze) : maze_(std::move(maze)) { publish(); }

    void tick(scene::Scene& /*scene*/, float /*dt*/) override {}

private:
    void publish() const {
        runtime::set_ui_model("run", "{\"score\":" + std::to_string(score_) + "}");
    }

    std::optional<Maze> maze_;  // Coins (M3) and hunters (M4) drive on it.
    int score_ = 0;
};

std::unique_ptr<runtime::Instance> start_run(scene::Scene& scene,
                                             const runtime::StartContext& context) {
    std::string error;
    std::optional<Maze> maze = load_maze(scene, context.content_root, error);
    if (!maze) {
        std::fprintf(stderr, "coin_rush: %s\n", error.c_str());
    } else {
        for (const std::string& problem : check_maze(*maze)) {
            std::fprintf(stderr, "coin_rush: maze: %s\n", problem.c_str());
        }
    }
    return std::make_unique<Run>(std::move(maze));
}

}  // namespace

void register_game(doc::Registry& /*registry*/) {
    static bool added = false;  // Apps may build several registries.
    if (added) return;
    added = true;
    scene::register_component<MazeSpot>();  // Its rules: check_maze (tests, run start).
    runtime::add_system({"CoinRushRun", nullptr, start_run});
}

}  // namespace coin_rush
