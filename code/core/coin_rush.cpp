#include "core/coin_rush.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/coins.hpp"
#include "core/hunters.hpp"
#include "core/maze.hpp"
#include "robots/core/robots_module.hpp"
#include "runtime/session.hpp"
#include "scene/module_component.hpp"
#include "schema/coin_rush.gen.hpp"

namespace coin_rush {

namespace {

template <class T>
const T* find_component(const scene::Entity& e) {
    for (const auto& box : e.module_components) {
        if (const T* c = scene::module_component_as<T>(*box)) return c;
    }
    return nullptr;
}

// A coin as drawn: a flat disc turning on its upright axis.
void show_coin(scene::Entity& e, const Level& level) {
    e.mesh = scene::Mesh{};
    e.mesh->mesh = "sphere";
    e.mesh->material = level.material;
}

// One run of Coin Rush: the maze (milestone 2), its coins, the level and the score the HUD
// shows (milestone 3), and the hunters (milestone 4): a touch from one ends the run, the score
// stays. The run flow (M5) adds the countdown, results and Play Again later.
//
// The hunters drive on autopilot (random exits) until engine request #3 lets game code steer
// them with the brains in hunters.hpp (docs/hunters.md).
class Run : public runtime::Instance {
public:
    Run(scene::Scene& scene, std::optional<Maze> maze) : maze_(std::move(maze)) {
        for (const scene::Entity& e : scene.entities) {
            const robots::Robot* robot = find_component<robots::Robot>(e);
            if (!robot) continue;
            if (robot->player && player_.empty()) {
                player_ = e.id;
                player_name_ = e.name;
            }
            if (!robot->player && hunter_from_name(e.name)) ++hunters_;
        }
        if (maze_) {
            board_.emplace(coin_spots(maze_->graph));
            for (size_t i = 0; i < board_->spots().size(); ++i) {
                scene::Entity& coin = scene.add_entity("Coin " + std::to_string(i + 1));
                coin.transform.position = board_->spots()[i].position;
                coin.transform.scale = {0.3f, 0.3f, 0.08f};  // 0.6 m across (sphere: 2 m).
                coin.spin = scene::Spin{};
                coin.spin->speed = {0.0f, 120.0f, 0.0f};
                show_coin(coin, board_->level_info());
                coins_.push_back(coin.id);
            }
        }
        publish();
    }

    void tick(scene::Scene& scene, float /*dt*/) override {
        if (!board_ || player_.empty() || caught_) return;
        if (check_catch()) {
            publish();
            return;
        }
        const scene::Entity* player = scene.find(player_);
        if (!player) return;
        const CoinBoard::Pickup pickup = board_->collect(player->transform.position);
        if (pickup.taken.empty()) return;
        for (int i : pickup.taken) {
            if (scene::Entity* coin = scene.find(coins_[static_cast<size_t>(i)])) coin->mesh.reset();
        }
        if (pickup.level_cleared && !board_->won()) {  // Every coin back, next colour.
            for (const std::string& id : coins_) {
                if (scene::Entity* coin = scene.find(id)) show_coin(*coin, board_->level_info());
            }
        }
        publish();
    }

private:
    // A hunter touched the player since the last tick (robots' contact log).
    bool check_catch() {
        int64_t newest = last_contact_;
        for (const robots::ContactEvent& c : robots::contact_log()) {
            const auto tick = static_cast<int64_t>(c.tick);
            if (tick <= last_contact_) continue;
            newest = std::max(newest, tick);
            if (!caught_ && is_catch(c.a, c.b, player_name_)) {
                caught_ = true;
                caught_by_ = c.a == player_name_ ? c.b : c.a;
            }
        }
        last_contact_ = newest;
        return caught_;
    }

    void publish() const {
        const int score = board_ ? board_->score() : 0;
        const int level = board_ ? board_->level() : 1;
        const int left = board_ ? board_->left() : 0;
        const bool won = board_ && board_->won();
        runtime::set_ui_model("run", "{\"score\":" + std::to_string(score) +
                                         ",\"level\":" + std::to_string(level) +
                                         ",\"coins_left\":" + std::to_string(left) +
                                         ",\"won\":" + (won ? "true" : "false") +
                                         ",\"caught\":" + (caught_ ? "true" : "false") +
                                         ",\"caught_by\":\"" + caught_by_ + "\"" +
                                         ",\"hunters\":" + std::to_string(hunters_) + "}");
    }

    std::optional<Maze> maze_;  // Hunters (M4) drive on it.
    std::optional<CoinBoard> board_;
    std::vector<std::string> coins_;  // Coin entity per board spot.
    std::string player_;              // The player robot's entity.
    std::string player_name_;         // ...and its name (contacts name robots).
    int hunters_ = 0;                 // Hunter robots in the scene (Blinky, Pinky, ...).
    int64_t last_contact_ = -1;       // Newest contact tick looked at.
    bool caught_ = false;
    std::string caught_by_;           // The hunter that caught the player.
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
    return std::make_unique<Run>(scene, std::move(maze));
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
