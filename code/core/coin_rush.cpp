#include "core/coin_rush.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/coins.hpp"
#include "core/hunters.hpp"
#include "core/map.hpp"
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

// The best score so far. Kept while the app runs (Play Again, the editor's Play / Stop); saving
// it on the phone waits for the engine's save data (docs/engine-requests/save-data.md).
int g_best_score = 0;

// The run flow (milestone 5, docs/run-flow.md): the start screen, a 3-2-1 countdown with
// everything frozen, play, pause, and the results screen with Play Again.
enum class Phase { Menu, Countdown, Playing, Paused, Over };

const char* phase_name(Phase phase) {
    switch (phase) {
        case Phase::Menu: return "menu";
        case Phase::Countdown: return "countdown";
        case Phase::Playing: return "playing";
        case Phase::Paused: return "paused";
        case Phase::Over: return "over";
    }
    return "menu";
}

constexpr int64_t kCountdownTicks = 3 * 60;  // 3-2-1, one second each.

// The way of the player's queued premove ("left early" -> "left"; "" = none).
std::string plan_way(const std::string& plan) {
    const size_t space = plan.find(' ');
    return space == std::string::npos ? plan : plan.substr(0, space);
}

// The wall boxes of the maze (tools/maze_layout.py: "Wall NN" cubes, not rotated), on the
// ground plane.
std::vector<MapRect> wall_rects(const scene::Scene& scene) {
    std::vector<MapRect> walls;
    for (const scene::Entity& e : scene.entities) {
        if (e.name.rfind("Wall ", 0) != 0 || !e.mesh || e.mesh->mesh != "cube") continue;
        const glm::vec3 c = e.transform.position, h = glm::abs(e.transform.scale);
        walls.push_back({c.x - h.x, c.z - h.z, c.x + h.x, c.z + h.z});
    }
    return walls;
}

std::string number(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    return buf;
}

// Coin Rush: the maze (milestone 2), its coins, the level and the score the HUD shows
// (milestone 3), the hunters (milestone 4, docs/hunters.md), the run flow (milestone 5,
// docs/run-flow.md) and the hold-to-view map (milestone 6, docs/map.md). The session opens on the start screen with everything held. Play (or
// Play Again) resets the board and the hunters, parks the player on a random central spawn and
// counts 3-2-1; then each hunter waits at home until its release, scatters to its corner and
// hunts with its own brain, faster on each level. A touch from one ("Caught!") or the last coin
// of level 5 ("Cleared!") ends the run: everything stands still and the results show the score
// and the best.
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
                hunters_.push_back({e.id, *which, e.transform.position, robot->start_port});
            }
        }
        if (maze_) {
            board_.emplace(coin_spots(maze_->graph));
            for (size_t i = 0; i < board_->spots().size(); ++i) {
                scene::Entity& coin = scene.add_entity("Coin " + std::to_string(i + 1));
                coin.transform.position = board_->spots()[i].position;
                coin.transform.scale = {0.3f, 0.3f, 0.08f};  // 0.6 m across (sphere: 2 m).
                show_coin(coin, board_->level_info());
                coins_.push_back(coin.id);
            }
            start_hunters();
            map_ = build_map(maze_->graph, wall_rects(scene), board_->spots());
            runtime::set_ui_model("map", map_layout_json(*map_));
            publish_coins();
        }
        robots::set_held("", true);  // The start screen: nothing moves until Play.
        spin_coins(scene, true);
        publish();
    }

    void tick(scene::Scene& scene, float /*dt*/) override {
        ++clock_;
        if (!board_ || player_.empty()) return;
        if (phase_ == Phase::Countdown) {
            if (--countdown_ <= 0) go();
            publish();
            return;
        }
        if (phase_ != Phase::Playing) return;
        if (check_catch()) {
            end_run(false);
            return;
        }
        const scene::Entity* player = scene.find(player_);
        if (!player) return;
        steer_hunters(scene, player->transform.position);
        if (map_open_ && ticks_ % 2 == 0) publish_map_live(scene);
        const robots::RailRider* rider = find_component<robots::RailRider>(*player);
        std::string plan = rider ? plan_way(rider->plan) : "";
        bool changed = plan != plan_;
        plan_ = std::move(plan);
        const CoinBoard::Pickup pickup = board_->collect(player->transform.position);
        for (int i : pickup.taken) {
            if (scene::Entity* coin = scene.find(coins_[static_cast<size_t>(i)])) coin->mesh.reset();
            changed = true;
        }
        if (pickup.level_cleared && board_->won()) {
            end_run(true);
            return;
        }
        if (pickup.level_cleared) {  // Every coin back, next colour.
            for (const std::string& id : coins_) {
                if (scene::Entity* coin = scene.find(id)) show_coin(*coin, board_->level_info());
            }
            set_hunter_speed();
        }
        if (!pickup.taken.empty() || pickup.level_cleared) publish_coins();
        if (changed) publish();
    }

    // The UI's buttons: "play" (start screen, results), "pause", "resume", "menu", and the map
    // button held down ("map_open") and let go ("map_close").
    void ui_request(scene::Scene& scene, const std::string& name,
                    const std::string& /*json*/) override {
        if (name == "play" && (phase_ == Phase::Menu || phase_ == Phase::Over ||
                               phase_ == Phase::Paused)) {
            new_run(scene);
        } else if (name == "pause" && phase_ == Phase::Playing) {
            pause(scene);
        } else if (name == "resume" && phase_ == Phase::Paused) {
            resume(scene);
        } else if (name == "menu" && (phase_ == Phase::Over || phase_ == Phase::Paused)) {
            phase_ = Phase::Menu;
            robots::set_held("", true);
            spin_coins(scene, true);
            publish();
        } else if (name == "map_open" && phase_ == Phase::Playing) {
            open_map(scene, true);
        } else if (name == "map_close") {
            open_map(scene, false);
        }
    }

    // Desktop shortcuts: Space / Enter = Play (Again), Escape / P = pause and resume.
    void key(scene::Scene& scene, const runtime::KeyEvent& event) override {
        if (event.key == "Space" || event.key == "Return") {
            if (phase_ == Phase::Menu || phase_ == Phase::Over) new_run(scene);
        } else if (event.key == "Escape" || event.key == "P") {
            if (phase_ == Phase::Playing) {
                pause(scene);
            } else if (phase_ == Phase::Paused) {
                resume(scene);
            }
        }
    }

private:
    struct HunterBot {
        std::string id;  // Entity id.
        Hunter which;
        glm::vec3 home{0.0f};  // Its home corner (where it starts).
        int home_port = 0;     // Robot.start_port: parked at home as if arrived by it.
        bool released = false;
    };
    // Where each hunter aims (HunterBot order), read by its driver in the Robots tick.
    using Targets = std::vector<glm::vec3>;

    // Give each hunter its brain as the robot's driver.
    void start_hunters() {
        targets_ = std::make_shared<Targets>();
        for (size_t i = 0; i < hunters_.size(); ++i) {
            targets_->push_back(hunters_[i].home);
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

    // Coins turn on the menu, in play and on the results; they stop while paused.
    void spin_coins(scene::Scene& scene, bool on) {
        for (const std::string& id : coins_) {
            scene::Entity* coin = scene.find(id);
            if (!coin) continue;
            if (on) {
                coin->spin = scene::Spin{};
                coin->spin->speed = {0.0f, 120.0f, 0.0f};
            } else {
                coin->spin.reset();
            }
        }
    }

    // The hold-to-view map (docs/map.md): while it is open the player's robot takes no input.
    // A driver that never answers does that: swipes are ignored (a driven robot ignores them),
    // a premove queued before opening is still taken, and without one the robot stops at the
    // next intersection, as it would with no swipe. Letting go gives the swipes back.
    void open_map(scene::Scene& scene, bool open) {
        if (open == map_open_ || player_.empty()) return;
        map_open_ = open;
        if (open) {
            robots::set_driver(player_, [](const robots::DriveRequest&) {
                return std::optional<robots::Swipe>{};
            });
            publish_map_live(scene);
        } else {
            robots::set_driver(player_, {});
        }
        publish();
    }

    // Play / Play Again: a fresh board on level 1, score 0, the hunters back home, the player on
    // a random central spawn, then the countdown. Everything is held until "go".
    void new_run(scene::Scene& scene) {
        if (!board_) return;
        open_map(scene, false);
        board_.emplace(coin_spots(maze_->graph));
        for (const std::string& id : coins_) {
            if (scene::Entity* coin = scene.find(id)) show_coin(*coin, board_->level_info());
        }
        spin_coins(scene, true);
        robots::set_held("", true);
        for (HunterBot& h : hunters_) {
            h.released = false;
            robots::place_robot(h.id, "", h.home_port);
        }
        set_hunter_speed();
        publish_coins();
        // The spawn's seed: ticks since the session started (how long the player waited on a
        // screen), so it differs between runs but stays the same for the same inputs.
        const auto seed = static_cast<uint32_t>(clock_ * 2654435761u) ^ (++runs_ * 40503u);
        spawn_ = pick_spawn(*maze_, seed);
        if (!spawn_.empty()) {
            int ports = 1;
            for (const rails::Node& n : maze_->graph.nodes) {
                if (n.id == spawn_) ports = std::max(1, static_cast<int>(n.ports.size()));
            }
            robots::place_robot(player_, spawn_, static_cast<int>((seed >> 8) % ports));
        }
        ticks_ = 0;
        caught_ = false;
        caught_by_.clear();
        new_best_ = false;
        plan_.clear();
        player_forward_ = {0.0f, 0.0f, -1.0f};
        countdown_ = kCountdownTicks;
        phase_ = Phase::Countdown;
        publish();
    }

    // The countdown is over: the player may drive, the hunters' release clock starts.
    void go() {
        phase_ = Phase::Playing;
        robots::set_held(player_, false);
        last_contact_ = newest_contact();  // Touches from before the run don't count.
    }

    void pause(scene::Scene& scene) {
        open_map(scene, false);
        phase_ = Phase::Paused;
        robots::set_held("", true);
        spin_coins(scene, false);
        publish();
    }

    // Back to play: the player and the hunters already out start again (0.5 s launch).
    void resume(scene::Scene& scene) {
        phase_ = Phase::Playing;
        robots::set_held(player_, false);
        for (const HunterBot& h : hunters_) {
            if (h.released) robots::set_held(h.id, false);
        }
        spin_coins(scene, true);
        publish();
    }

    // Caught! or Cleared!: everything stands still, the score stays, the best is updated.
    void end_run(bool cleared) {
        if (map_open_) {
            map_open_ = false;
            robots::set_driver(player_, {});
        }
        phase_ = Phase::Over;
        robots::set_held("", true);
        if (cleared) robots::trigger_robot_anim("victory", player_);
        const int score = board_->score();
        new_best_ = score > g_best_score;
        if (new_best_) g_best_score = score;
        plan_.clear();
        publish();
    }

    // Releases hunters on time and points each at its target for this tick.
    void steer_hunters(const scene::Scene& scene, const glm::vec3& player) {
        ++ticks_;
        const glm::vec3 step = player - player_pos_;  // The way the player is heading.
        if (glm::dot(step, step) > 1e-6f && glm::dot(step, step) < 4.0f) player_forward_ = step;
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

    static int64_t newest_contact() {
        int64_t newest = -1;
        for (const robots::ContactEvent& c : robots::contact_log()) {
            newest = std::max(newest, static_cast<int64_t>(c.tick));
        }
        return newest;
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
        const int count = phase_ == Phase::Countdown
                              ? static_cast<int>((countdown_ + 59) / 60)  // 3, 2, 1.
                              : 0;
        const auto flag = [](bool b) { return b ? "true" : "false"; };
        runtime::set_ui_model(
            "run", "{\"score\":" + std::to_string(score) + ",\"level\":" + std::to_string(level) +
                       ",\"coins_left\":" + std::to_string(left) + ",\"won\":" + flag(won) +
                       ",\"caught\":" + flag(caught_) + ",\"caught_by\":\"" + caught_by_ + "\"" +
                       ",\"hunters\":" + std::to_string(hunters_.size()) + ",\"phase\":\"" +
                       phase_name(phase_) + "\",\"countdown\":" + std::to_string(count) +
                       ",\"best\":" + std::to_string(g_best_score) +
                       ",\"new_best\":" + flag(new_best_) + ",\"plan\":\"" + plan_ + "\"" +
                       ",\"spawn\":\"" + spawn_ + "\"" +
                       ",\"map_open\":" + flag(map_open_) + "}");
    }

    // The map's coins: which spots are still out ("1") or taken ("0"), and the level (colour).
    void publish_coins() const {
        if (!board_) return;
        std::string out;
        out.reserve(board_->spots().size());
        for (size_t i = 0; i < board_->spots().size(); ++i) {
            out += board_->out(static_cast<int>(i)) ? '1' : '0';
        }
        runtime::set_ui_model("map_coins", "{\"level\":" + std::to_string(board_->level()) +
                                               ",\"out\":\"" + out + "\"}");
    }

    // What moves on the open map: the player (where, heading) and the hunters, in map
    // coordinates. Sent every other tick while the map is open, never while it is shut.
    void publish_map_live(const scene::Scene& scene) const {
        if (!map_) return;
        const scene::Entity* player = scene.find(player_);
        if (!player) return;
        const glm::vec2 at = to_map(*map_, player->transform.position);
        // The way the robot faces: its local -z turned by its yaw (the maze is flat).
        const float yaw = glm::radians(player->transform.rotation.y);
        const glm::vec2 heading(-std::sin(yaw), -std::cos(yaw));
        std::string out = "{\"x\":" + number(at.x) + ",\"z\":" + number(at.y) +
                          ",\"hx\":" + number(heading.x) + ",\"hz\":" + number(heading.y) +
                          ",\"hunters\":[";
        bool first = true;
        for (const HunterBot& h : hunters_) {
            const scene::Entity* e = scene.find(h.id);
            if (!e) continue;
            const glm::vec2 p = to_map(*map_, e->transform.position);
            if (!first) out += ',';
            first = false;
            out += "{\"name\":\"" + e->name + "\",\"x\":" + number(p.x) +
                   ",\"z\":" + number(p.y) + "}";
        }
        out += "]}";
        runtime::set_ui_model("map_live", out);
    }

    std::optional<Maze> maze_;
    std::optional<CoinBoard> board_;
    std::optional<MapLayout> map_;    // The hold-to-view map's drawing (docs/map.md).
    bool map_open_ = false;           // The map button is held: the player takes no input.
    std::vector<std::string> coins_;  // Coin entity per board spot.
    std::string player_;              // The player robot's entity.
    std::string player_name_;         // ...and its name (contacts name robots).
    float player_speed_ = 9.8f;       // Its top speed (hunter speeds are shares of it).
    glm::vec3 player_pos_{0.0f};      // Last tick's position...
    glm::vec3 player_forward_{0.0f, 0.0f, -1.0f};  // ...and the way it last moved.
    std::vector<HunterBot> hunters_;  // Blinky, Pinky, Inky, Clyde as found in the scene.
    std::shared_ptr<Targets> targets_;
    Phase phase_ = Phase::Menu;
    int64_t clock_ = 0;               // Ticks since the session started (the spawn's seed).
    uint32_t runs_ = 0;               // Runs started in this session.
    int64_t countdown_ = 0;           // Ticks left in the 3-2-1.
    int64_t ticks_ = 0;               // Play ticks this run (the hunters' release clock).
    int64_t last_contact_ = -1;       // Newest contact tick looked at.
    bool caught_ = false;
    bool new_best_ = false;
    std::string caught_by_;           // The hunter that caught the player.
    std::string plan_;                // The player's queued premove: up / left / right / down.
    std::string spawn_;               // The node this run started on.
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
