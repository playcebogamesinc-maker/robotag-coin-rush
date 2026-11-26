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
// shows (milestone 3), and the hunters (milestone 4, docs/hunters.md): each waits at home until
// its release, scatters to its corner, then hunts with its own brain (hunters.hpp), faster on
// each level; a touch from one ends the run, everything stands still and the score stays. The
// run flow (M5) adds the countdown, results and Play Again later.
class Run : public runtime::Instance {
public:
    Run(scene::Scene& scene, std::optional<Maze> maze) : maze_(std::move(maze)) {
        for (const scene::Entity& e : scene.entities) {
            const robots::Robot* robot = find_component<robots::Robot>(e);
            if (!robot) continue;
            if (robot->player && player_.empty()) {
                player_ = e.id;
                player_name_ = e.name;
                player_speed_ = robot->top_speed;
                player_pos_ = e.transform.position;
            }
            if (robot->player) continue;
            if (std::optional<Hunter> which = hunter_from_name(e.name)) {
                hunters_.push_back({e.id, *which, e.transform.position});
            }
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
            start_hunters();
        }
        publish();
    }

    void tick(scene::Scene& scene, float /*dt*/) override {
        if (!board_ || player_.empty() || caught_) return;
        if (check_catch()) {
            robots::set_held("", true);  // Everything stands still.
            publish();
            return;
        }
        const scene::Entity* player = scene.find(player_);
        if (!player) return;
        steer_hunters(scene, player->transform.position);
        const CoinBoard::Pickup pickup = board_->collect(player->transform.position);
        if (pickup.taken.empty()) return;
        for (int i : pickup.taken) {
            if (scene::Entity* coin = scene.find(coins_[static_cast<size_t>(i)])) coin->mesh.reset();
        }
        if (pickup.level_cleared && !board_->won()) {  // Every coin back, next colour.
            for (const std::string& id : coins_) {
                if (scene::Entity* coin = scene.find(id)) show_coin(*coin, board_->level_info());
            }
            set_hunter_speed();
        }
        publish();
    }

private:
    struct HunterBot {
        std::string id;  // Entity id.
        Hunter which;
        glm::vec3 home{0.0f};  // Its home corner (where it starts).
        bool released = false;
    };
    // Where each hunter aims (HunterBot order), read by its driver in the Robots tick.
    using Targets = std::vector<glm::vec3>;

    // Hold every hunter at home and give each its brain as the robot's driver.
    void start_hunters() {
        targets_ = std::make_shared<Targets>();
        for (size_t i = 0; i < hunters_.size(); ++i) {
            targets_->push_back(hunters_[i].home);
            robots::set_held(hunters_[i].id, true);
            const std::shared_ptr<const Targets> targets = targets_;
            robots::set_driver(hunters_[i].id, [targets, i](const robots::DriveRequest& r) {
                return choose_swipe(*r.map, r.node, r.entry, (*targets)[i]);
            });
        }
        set_hunter_speed();
    }

    void set_hunter_speed() {
        const float speed = hunter_speed(board_ ? board_->level() : 1, player_speed_);
        for (const HunterBot& h : hunters_) robots::set_top_speed(h.id, speed);
    }

    // Releases hunters on time and points each at its target for this tick.
    void steer_hunters(const scene::Scene& scene, const glm::vec3& player) {
        ++ticks_;
        const glm::vec3 step = player - player_pos_;  // The way the player is heading.
        if (glm::dot(step, step) > 1e-6f) player_forward_ = step;
        player_pos_ = player;
        glm::vec3 blinky = player;
        for (const HunterBot& h : hunters_) {
            const scene::Entity* e = scene.find(h.id);
            if (e && h.which == Hunter::Blinky) blinky = e->transform.position;
        }
        for (size_t i = 0; i < hunters_.size(); ++i) {
            HunterBot& h = hunters_[i];
            const HunterMode mode = hunter_mode(h.which, ticks_);
            if (mode != HunterMode::Home && !h.released) {
                h.released = true;
                robots::set_held(h.id, false);
            }
            const scene::Entity* e = scene.find(h.id);
            Sighting seen;
            seen.player = player;
            seen.player_forward = player_forward_;
            seen.blinky = blinky;
            seen.self = e ? e->transform.position : h.home;
            seen.home = h.home;
            (*targets_)[i] = hunter_target(h.which, mode, seen);
        }
    }

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
                                         ",\"hunters\":" + std::to_string(hunters_.size()) +
                                         "}");
    }

    std::optional<Maze> maze_;
    std::optional<CoinBoard> board_;
    std::vector<std::string> coins_;  // Coin entity per board spot.
    std::string player_;              // The player robot's entity.
    std::string player_name_;         // ...and its name (contacts name robots).
    float player_speed_ = 9.8f;       // Its top speed (hunter speeds are shares of it).
    glm::vec3 player_pos_{0.0f};      // Last tick's position...
    glm::vec3 player_forward_{0.0f, 0.0f, -1.0f};  // ...and the way it last moved.
    std::vector<HunterBot> hunters_;  // Blinky, Pinky, Inky, Clyde as found in the scene.
    std::shared_ptr<Targets> targets_;
    int64_t ticks_ = 0;               // Run ticks so far (the hunters' release clock).
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
