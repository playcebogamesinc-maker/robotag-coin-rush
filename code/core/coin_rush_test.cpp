// coin_rush_test: Coin Rush's start scene loads and plays with the game's code, and its maze
// keeps the layout rules (docs/gameplay.md "The arena", docs/maze.md). Exit code 0 = pass
// (ctest, label game:coin_rush).

#include "core/coin_rush.hpp"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "all_modules.hpp"
#include "core/coins.hpp"
#include "core/maze.hpp"
#include "rails/core/rail_bake.hpp"
#include "document/game.hpp"
#include "project/project.hpp"
#include "robots/core/robot_motion.hpp"
#include "robots/core/robots_module.hpp"
#include "runtime/session.hpp"
#include "scene/scene.hpp"

namespace {

int failures = 0;

#define CHECK(cond, ...)                       \
    do {                                       \
        if (!(cond)) {                         \
            std::printf("FAILED %s: ", #cond); \
            std::printf(__VA_ARGS__);          \
            std::printf("\n");                 \
            ++failures;                        \
        }                                      \
    } while (0)

// The latest "run" model a session published ("" = none).
std::string run_model(runtime::Session& session, std::string last = "") {
    for (const auto& [name, json] : session.take_ui_models()) {
        if (name == "run") last = json;
    }
    return last;
}

// The run starts on level 1 with a score of 0 and puts its coins in the scene.
void test_run(const scene::Scene& scene, const std::string& content) {
    runtime::Session session;
    session.start(scene, content);
    for (int i = 0; i < 120; ++i) session.tick();
    const std::string model = run_model(session);
    int coins = 0;
    for (const scene::Entity& e : session.scene().entities) {
        if (e.name.starts_with("Coin ") && e.mesh) ++coins;
    }
    session.stop();
    CHECK(model.starts_with("{\"score\":0,\"level\":1,"), "run model: %s", model.c_str());
    CHECK(coins > 100, "only %d coins in the running scene", coins);
}

// Coins line every rail at even spacing, off the nodes, one rail's coins in a row.
void test_coin_spots(const coin_rush::Maze& maze) {
    const std::vector<coin_rush::CoinSpot> spots = coin_rush::coin_spots(maze.graph);
    rails::BakeCache bakes;
    bakes.update(maze.graph);
    std::map<std::string, std::vector<float>> per_rail;
    for (const coin_rush::CoinSpot& c : spots) per_rail[c.rail].push_back(c.s);
    CHECK(per_rail.size() == maze.graph.edges.size(), "coins on %zu of %zu rails",
          per_rail.size(), maze.graph.edges.size());
    for (const auto& [rail, s] : per_rail) {
        const rails::EdgeBake* bake = bakes.find(rail);
        if (!bake) continue;
        const float gap = bake->length / static_cast<float>(s.size());
        CHECK(gap > 2.5f && gap < 3.5f, "rail %s: coins %.2f m apart", rail.c_str(),
              static_cast<double>(gap));
        CHECK(s.front() > 1.0f && bake->length - s.back() > 1.0f, "rail %s: a coin on a node",
              rail.c_str());
    }
    for (const rails::Node& n : maze.graph.nodes) {  // Nothing sits on a node (spawns).
        for (const coin_rush::CoinSpot& c : spots) {
            const glm::vec2 d(c.position.x - n.data.position.x, c.position.z - n.data.position.z);
            CHECK(glm::length(d) > coin_rush::kPickupRadius, "coin on rail %s within reach of node %s",
                  c.rail.c_str(), n.id.c_str());
        }
    }
    std::printf("coin_rush_test: %zu coins on %zu rails\n", spots.size(), per_rail.size());
}

// Levels: clearing the board brings every coin back worth double, level 5 wins, a coin is
// picked up only once.
void test_levels(const coin_rush::Maze& maze) {
    coin_rush::CoinBoard board(coin_rush::coin_spots(maze.graph));
    const int n = static_cast<int>(board.spots().size());
    const glm::vec3 first = board.spots()[0].position;
    CHECK(board.collect(first).points == 1, "the first coin isn't worth 1");
    CHECK(board.collect(first).points == 0, "a coin was picked up twice");
    CHECK(board.collect(glm::vec3(1000.0f)).taken.empty(), "a coin out of reach was picked up");
    int expected = 0;
    for (int level = 1; level <= 5; ++level) {
        CHECK(board.level() == level, "level %d, expected %d", board.level(), level);
        CHECK(board.level_info().points == 1 << (level - 1), "level %d is worth %d", level,
              board.level_info().points);
        expected += n << (level - 1);
        bool cleared = false;
        for (int i = 0; i < n; ++i) {
            if (board.out(i)) cleared |= board.collect(board.spots()[static_cast<size_t>(i)].position).level_cleared;
        }
        CHECK(cleared, "level %d didn't clear", level);
        CHECK(board.score() == expected, "score %d after level %d, expected %d", board.score(),
              level, expected);
        if (level < 5) CHECK(board.left() == n, "level %d: %d coins back of %d", level + 1, board.left(), n);
    }
    CHECK(board.won(), "clearing level 5 didn't win the run");
    CHECK(board.collect(first).points == 0, "a coin was picked up after the win");
}

// Driving picks coins up: the player swipes away from the spawn, the score goes up and the
// coins it drove over are gone from the scene.
void test_drive_picks_up_coins(const scene::Scene& scene, const std::string& content) {
    int best = 0;
    for (const float dy : {-300.0f, 300.0f}) {
        runtime::Session session;
        session.start(scene, content);
        session.tick();
        const float w = 540.0f, h = 1170.0f;
        session.pointer({runtime::PointerEvent::Type::Down, 270.0f, 600.0f, w, h});
        session.pointer({runtime::PointerEvent::Type::Move, 270.0f, 600.0f + dy * 0.5f, w, h});
        session.pointer({runtime::PointerEvent::Type::Up, 270.0f, 600.0f + dy, w, h});
        std::string model;
        for (int i = 0; i < 4 * 60; ++i) {
            session.tick();
            model = run_model(session, model);
        }
        int hidden = 0;
        for (const scene::Entity& e : session.scene().entities) {
            if (e.name.starts_with("Coin ") && !e.mesh) ++hidden;
        }
        session.stop();
        int score = 0;
        std::sscanf(model.c_str(), "{\"score\":%d", &score);
        CHECK(score == hidden, "score %d but %d coins gone", score, hidden);
        best = score > best ? score : best;
    }
    std::printf("coin_rush_test: a 4 s drive picked up %d coins\n", best);
    CHECK(best >= 3, "driving from the spawn picked up %d coins", best);
}

// The start scene's maze keeps every layout rule, and the player starts on a spawn.
void test_maze_rules(const coin_rush::Maze& maze, const scene::Scene& scene) {
    for (const std::string& problem : coin_rush::check_maze(maze)) {
        CHECK(false, "maze: %s", problem.c_str());
    }
    CHECK(maze.spawns.size() >= 2, "only %zu spawns", maze.spawns.size());
    std::set<std::string> picked;
    for (uint32_t seed = 0; seed < 64; ++seed) picked.insert(coin_rush::pick_spawn(maze, seed));
    CHECK(picked.size() == maze.spawns.size(), "64 seeds picked %zu of %zu spawns", picked.size(),
          maze.spawns.size());
    bool player_on_spawn = false;
    for (const scene::Entity& e : scene.entities) {
        for (const auto& box : e.module_components) {
            const robots::Robot* r = scene::module_component_as<robots::Robot>(*box);
            if (!r || !r->player) continue;
            for (const std::string& s : maze.spawns) player_on_spawn |= r->start_node == s;
        }
    }
    CHECK(player_on_spawn, "the player robot doesn't start on a spawn");
}

// The checks catch a broken layout: a removed rail leaves dead ends.
void test_maze_check_catches_dead_end(coin_rush::Maze maze) {
    maze.graph.edges.pop_back();
    CHECK(!coin_rush::check_maze(maze).empty(), "a missing rail wasn't reported");
}

// Drives the whole maze from the player's start without premoves (stopping at every
// intersection), always heading for the nearest rail not driven yet. Every rail is reachable,
// corners are driven through, and the tour fits the "about a minute per level" goal.
void test_drive_every_rail(const coin_rush::Maze& maze, const std::string& start) {
    robots::RailMap map;
    map.build(maze.graph);
    const int start_node = map.find_node(start);
    CHECK(start_node >= 0, "no start node");
    if (start_node < 0) return;
    std::set<std::string> todo;
    for (const rails::Edge& e : maze.graph.edges) todo.insert(e.id);

    // Drives from a parked state after a swipe until parked again (or 20 s); returns the
    // rails driven, or nullopt if it doesn't leave.
    auto leg = [&](robots::Rider& rider, robots::Swipe s, int& ticks,
                   bool& stopped_on_corner) -> std::optional<std::set<std::string>> {
        robots::swipe(map, rider, s);
        std::set<std::string> rails;
        for (int i = 0; i < 20 * 60; ++i) {
            robots::step(map, rider);
            ++ticks;
            if (!rider.parked) {
                const robots::RailMap::Track& t = map.tracks()[static_cast<size_t>(rider.track)];
                if (!t.junction) rails.insert(t.id);
            } else if (i > 0) {
                stopped_on_corner |= !map.nodes()[static_cast<size_t>(rider.node)].intersection();
                return rails;
            }
        }
        return rails.empty() ? std::nullopt : std::optional(rails);
    };

    robots::Rider rider = robots::park_rider(map, start_node, 0, 9.8f);
    int ticks = 0;
    bool stopped_on_corner = false;
    const robots::Swipe swipes[] = {robots::Swipe::Up, robots::Swipe::Left, robots::Swipe::Right,
                                    robots::Swipe::Down};
    for (int legs = 0; !todo.empty() && legs < 200; ++legs) {
        // Each option: how many new rails it drives, else how close it ends to one (BFS).
        int best = -1;
        int best_score = -1000000;
        for (int k = 0; k < 4; ++k) {
            robots::Rider trial = rider;
            int t = 0;
            bool corner = false;
            const std::optional<std::set<std::string>> rails = leg(trial, swipes[k], t, corner);
            if (!rails || !trial.parked) continue;
            int fresh = 0;
            for (const std::string& r : *rails) fresh += todo.count(r) ? 1 : 0;
            int score = fresh * 1000;
            if (fresh == 0) {  // Hops (through nodes) to the nearest rail not driven yet.
                std::map<int, int> hops{{trial.node, 0}};
                std::queue<int> q;
                q.push(trial.node);
                int found = 999;
                while (!q.empty() && found == 999) {
                    const int n = q.front();
                    q.pop();
                    for (int track : map.nodes()[static_cast<size_t>(n)].port_track) {
                        if (track < 0) continue;
                        const auto& tr = map.tracks()[static_cast<size_t>(track)];
                        if (todo.count(tr.id)) {
                            found = hops[n];
                            break;
                        }
                        const int next = tr.ends[0].node == n ? tr.ends[1].node : tr.ends[0].node;
                        if (!hops.count(next)) {
                            hops[next] = hops[n] + 1;
                            q.push(next);
                        }
                    }
                }
                score = -found;
            }
            if (score > best_score) {
                best_score = score;
                best = k;
            }
        }
        CHECK(best >= 0, "stuck at node %d", rider.node);
        if (best < 0) return;
        const std::optional<std::set<std::string>> rails =
            leg(rider, swipes[best], ticks, stopped_on_corner);
        for (const std::string& r : *rails) todo.erase(r);
    }
    const float seconds = static_cast<float>(ticks) / 60.0f;
    std::printf(
        "coin_rush_test: drove every rail in %.0f s (no premoves, stops at every "
        "intersection)\n",
        static_cast<double>(seconds));
    CHECK(todo.empty(), "%zu rails never driven", todo.size());
    CHECK(!stopped_on_corner, "the robot stopped on a corner (two-port) node");
    CHECK(seconds < 120.0f, "covering the maze took %.0f s", static_cast<double>(seconds));
}

}  // namespace

int main() {
    doc::Registry registry;
    modules::register_all_modules(registry);
    {
        const doc::GameScope scope("coin_rush");  // What the engine's game list does.
        coin_rush::register_game(registry);
    }

    std::string error;
    std::optional<project::OpenProject> project = project::load_project(RT_GAME_DIR, error);
    std::optional<scene::Scene> scene;
    if (project && project::activate_game(*project, error)) {
        scene = scene::load_scene(project->content_path(project->info.start_scene), error);
    }
    if (!scene) {
        std::printf("FAILED: %s\n", error.c_str());
        return EXIT_FAILURE;
    }
    test_run(*scene, project->content);

    std::optional<coin_rush::Maze> maze = coin_rush::load_maze(*scene, project->content, error);
    CHECK(maze.has_value(), "%s", error.c_str());
    if (maze) {
        test_maze_rules(*maze, *scene);
        test_maze_check_catches_dead_end(*maze);
        test_coin_spots(*maze);
        test_levels(*maze);
        test_drive_picks_up_coins(*scene, project->content);
        std::string start;
        for (const scene::Entity& e : scene->entities) {
            for (const auto& box : e.module_components) {
                if (const auto* r = scene::module_component_as<robots::Robot>(*box)) {
                    if (r->player) start = r->start_node;
                }
            }
        }
        test_drive_every_rail(*maze, start);
    }

    if (failures) return EXIT_FAILURE;
    std::printf("coin_rush_test: all passed\n");
    return EXIT_SUCCESS;
}
