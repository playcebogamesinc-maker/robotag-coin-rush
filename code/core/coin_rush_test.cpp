// coin_rush_test: Coin Rush's start scene loads and plays with the game's code, and its maze
// keeps the layout rules (docs/gameplay.md "The arena", docs/maze.md). Exit code 0 = pass
// (ctest, label game:coin_rush).

#include "core/coin_rush.hpp"

#include <algorithm>
#include <cmath>
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
#include "core/hunters.hpp"
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

// Presses Play on the start screen and waits out the 3-2-1: the run is on.
std::string start_playing(runtime::Session& session, std::string model = "") {
    session.ui_request("play", "{}");
    for (int i = 0; i < 3 * 60 + 1; ++i) {
        session.tick();
        model = run_model(session, model);
    }
    return model;
}

// A JSON field's text in a flat model ("" = missing): "score" -> "12", "phase" -> "playing".
std::string field(const std::string& json, const std::string& name) {
    const std::string key = "\"" + name + "\":";
    const size_t at = json.find(key);
    if (at == std::string::npos) return "";
    size_t begin = at + key.size();
    if (json[begin] == '"') {
        ++begin;
        return json.substr(begin, json.find('"', begin) - begin);
    }
    return json.substr(begin, json.find_first_of(",}", begin) - begin);
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
        start_playing(session);
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

// Hunter rules: release one by one, scatter then chase, speed per level, each personality's
// target, names and catches (docs/hunters.md).
void test_hunter_rules() {
    using coin_rush::Hunter;
    using coin_rush::HunterMode;
    const coin_rush::HunterRules rules;
    const Hunter all[] = {Hunter::Blinky, Hunter::Pinky, Hunter::Inky, Hunter::Clyde};
    for (int i = 0; i < 4; ++i) {
        const int64_t release = rules.first_release_ticks + i * rules.release_gap_ticks;
        CHECK(coin_rush::hunter_mode(all[i], release - 1) == HunterMode::Home, "hunter %d", i);
        CHECK(coin_rush::hunter_mode(all[i], release) == HunterMode::Scatter, "hunter %d", i);
        CHECK(coin_rush::hunter_mode(all[i], release + rules.scatter_ticks) == HunterMode::Chase,
              "hunter %d", i);
        CHECK(coin_rush::hunter_from_name(coin_rush::hunter_name(all[i])) == all[i], "name %d", i);
    }
    CHECK(coin_rush::hunter_mode(Hunter::Clyde, 100000) == HunterMode::Chase, "late in a run");
    float last = 0.0f;
    for (int level = 1; level <= 5; ++level) {
        const float v = coin_rush::hunter_speed(level, 9.8f);
        CHECK(v > last, "level %d: %.2f m/s", level, static_cast<double>(v));
        last = v;
    }
    CHECK(coin_rush::hunter_speed(1, 9.8f) < 0.7f * 9.8f, "level 1 isn't well below the player");
    CHECK(std::abs(coin_rush::hunter_speed(5, 9.8f) - 9.8f) < 0.01f, "level 5 isn't player speed");

    coin_rush::Sighting seen;
    seen.player = {0.0f, 0.0f, 0.0f};
    seen.player_forward = {1.0f, 0.0f, 0.0f};  // Heading +x.
    seen.blinky = {-6.0f, 0.0f, 0.0f};
    seen.self = {0.0f, 0.0f, 30.0f};
    seen.home = {27.0f, 0.0f, 27.0f};
    const auto target = [&](Hunter h, HunterMode m = HunterMode::Chase) {
        return coin_rush::hunter_target(h, m, seen, rules);
    };
    const auto near = [](glm::vec3 a, glm::vec3 b) { return glm::distance(a, b) < 0.01f; };
    CHECK(near(target(Hunter::Blinky), seen.player), "Blinky aims at the player");
    CHECK(near(target(Hunter::Pinky), {12.0f, 0.0f, 0.0f}), "Pinky aims 4 tiles ahead");
    CHECK(near(target(Hunter::Inky), {18.0f, 0.0f, 0.0f}), "Inky: (ahead 2 tiles) x 2 - Blinky");
    CHECK(near(target(Hunter::Clyde), seen.player), "Clyde far away hunts");
    CHECK(near(target(Hunter::Pinky, HunterMode::Scatter), seen.home), "scatter goes home");
    CHECK(near(target(Hunter::Blinky, HunterMode::Home), seen.home), "home stays home");
    seen.self = {0.0f, 0.0f, 10.0f};
    CHECK(near(target(Hunter::Clyde), seen.home), "Clyde up close goes home");
    seen.self = {0.0f, 0.0f, 3.0f};
    CHECK(near(target(Hunter::Pinky), seen.player), "close range: everyone aims at the player");
    CHECK(near(target(Hunter::Clyde), seen.player), "close range beats Clyde's shyness");

    CHECK(coin_rush::is_catch("Player", "Inky", "Player"), "player vs hunter");
    CHECK(coin_rush::is_catch("Clyde", "Player", "Player"), "hunter vs player");
    CHECK(!coin_rush::is_catch("Blinky", "Pinky", "Player"), "hunter vs hunter");
    CHECK(!coin_rush::is_catch("Player", "Bot", "Player"), "player vs another robot");
}

// At every intersection and every way in, a hunter takes a real exit, never back the way it
// came, and the one that leads closest to its target.
void test_choose_swipe(const coin_rush::Maze& maze) {
    robots::RailMap map;
    map.build(maze.graph);
    int checked = 0;
    for (size_t n = 0; n < map.nodes().size(); ++n) {
        const robots::RailMap::NodeInfo& info = map.nodes()[n];
        if (!info.intersection()) continue;
        const int node = static_cast<int>(n);
        for (int entry = 0; entry < static_cast<int>(info.node->ports.size()); ++entry) {
            if (info.turns[static_cast<size_t>(entry)].empty()) continue;
            // Aim at each exit's next intersection in turn: that exit must win.
            for (int exit : info.turns[static_cast<size_t>(entry)]) {
                const coin_rush::Stop stop = coin_rush::next_stop(map, node, exit);
                CHECK(stop.node >= 0 && stop.node != node, "node %d port %d leads nowhere",
                      node, exit);
                if (stop.node < 0) continue;
                const glm::vec3 target = map.nodes()[static_cast<size_t>(stop.node)].node->data.position;
                const std::optional<robots::Swipe> s =
                    coin_rush::choose_swipe(map, node, entry, target);
                CHECK(s && *s != robots::Swipe::Down, "node %d entry %d: no exit", node, entry);
                if (!s) continue;
                const int chosen = map.resolve(node, entry, *s);
                CHECK(chosen != entry, "node %d entry %d: turned back", node, entry);
                const coin_rush::Stop got = coin_rush::next_stop(map, node, chosen);
                CHECK(got.node == stop.node, "node %d entry %d: aimed at node %d, went to %d",
                      node, entry, stop.node, got.node);
                ++checked;
            }
        }
    }
    CHECK(checked > 20, "only %d choices checked", checked);
}

// A hunter driven by its brain (as its robot driver does in a run) leaves home after
// its release, never stops and never turns back; Blinky catches a player waiting on a spawn.
// (Pinky, Inky and Clyde aim around a player who doesn't move: in a run, Blinky is the one who
// punishes standing still.)
void test_hunters_catch(const coin_rush::Maze& maze) {
    robots::RailMap map;
    map.build(maze.graph);
    const coin_rush::HunterRules rules;
    const int spawn = map.find_node(maze.spawns.front());
    const robots::Rider player = robots::park_rider(map, spawn, 0, 9.8f);
    const robots::Pose player_pose = robots::pose(map, player);
    for (size_t h = 0; h < 4; ++h) {
        const auto hunter = static_cast<coin_rush::Hunter>(h);
        const int home = map.find_node(maze.homes[h]);
        CHECK(home >= 0, "%s has no home", coin_rush::hunter_name(hunter));
        if (home < 0) continue;
        const glm::vec3 home_pos = map.nodes()[static_cast<size_t>(home)].node->data.position;
        robots::Rider rider =
            robots::park_rider(map, home, 0, coin_rush::hunter_speed(3, 9.8f));
        int caught_at = -1, stops = 0, reverses = 0;
        float closest = 1e9f;
        bool left_home = false;
        for (int t = 0; t < 90 * 60 && caught_at < 0; ++t) {
            const coin_rush::HunterMode mode = coin_rush::hunter_mode(hunter, t, rules);
            if (mode != coin_rush::HunterMode::Home) {
                const bool deciding =
                    rider.parked ||
                    (!rider.plan && rider.brake == robots::Rider::Brake::None &&
                     !map.tracks()[static_cast<size_t>(rider.track)].junction);
                if (deciding) {
                    coin_rush::Sighting seen;
                    seen.player = player_pose.position;
                    seen.player_forward = player_pose.forward;
                    seen.blinky = seen.player;  // Inky: as if Blinky sat on the player.
                    seen.self = robots::pose(map, rider).position;
                    seen.home = home_pos;
                    const glm::vec3 target = coin_rush::hunter_target(hunter, mode, seen, rules);
                    const coin_rush::Stop at = coin_rush::decision_point(map, rider);
                    if (std::optional<robots::Swipe> s =
                            coin_rush::choose_swipe(map, at.node, at.entry, target)) {
                        robots::swipe(map, rider, *s);
                    }
                }
            }
            const int dir = rider.dir;
            const int track = rider.track;
            const bool was_parked = rider.parked;
            robots::step(map, rider);
            left_home |= !rider.parked;
            if (left_home && rider.parked && !was_parked) ++stops;
            if (!rider.parked && !was_parked && rider.track == track && rider.dir != dir) {
                ++reverses;
            }
            const float d = glm::distance(robots::pose(map, rider).position, player_pose.position);
            closest = std::min(closest, d);
            if (d < 1.0f) caught_at = t;
        }
        const char* name = coin_rush::hunter_name(hunter);
        std::printf("coin_rush_test: %s vs a waiting player: %s after %.1f s (closest %.1f m)\n",
                    name, caught_at >= 0 ? "caught" : "not caught",
                    static_cast<double>(caught_at) / 60.0, static_cast<double>(closest));
        CHECK(left_home, "%s never left home", name);
        CHECK(stops == 0, "%s stopped %d times", name, stops);
        CHECK(reverses == 0, "%s turned back %d times", name, reverses);
        if (hunter == coin_rush::Hunter::Blinky) {
            CHECK(caught_at >= 0 && caught_at < 30 * 60,
                  "Blinky didn't catch a player waiting on a spawn in 30 s");
        }
    }
}

// The start scene has the four hunters. In a run each waits at home until its release, then
// drives with its brain; hunters pass through each other; a hunter touching the player ends the
// run: everything stands still, the score stays and the model says who caught the player.
void test_run_with_hunters(const scene::Scene& scene, const std::string& content) {
    runtime::Session session;
    session.start(scene, content);
    const char* names[] = {"Blinky", "Pinky", "Inky", "Clyde"};
    const auto where = [&](const char* name) {
        for (const scene::Entity& e : session.scene().entities) {
            if (e.name == name) return e.transform.position;
        }
        return glm::vec3(-1000.0f);
    };
    std::vector<glm::vec3> homes;
    for (const char* n : names) homes.push_back(where(n));
    const auto moved = [&](int i) { return glm::distance(where(names[i]), homes[i]) > 0.5f; };

    std::string model = start_playing(session);
    int caught_tick = -1;
    bool held_early = true, blinky_first = false;
    for (int i = 0; i < 120 * 60 && caught_tick < 0; ++i) {
        session.tick();
        model = run_model(session, model);
        if (i == 50) held_early = !moved(0) && !moved(1) && !moved(2) && !moved(3);
        if (i == 120) blinky_first = moved(0) && !moved(1) && !moved(2) && !moved(3);
        if (model.find("\"caught\":true") != std::string::npos) caught_tick = i;
    }
    CHECK(model.find("\"hunters\":4") != std::string::npos, "run model: %s", model.c_str());
    CHECK(held_early, "a hunter left home before its release");
    CHECK(blinky_first, "Blinky isn't the only one out after 2 s");
    for (const robots::ContactEvent& c : robots::contact_log()) {
        CHECK(!(coin_rush::hunter_from_name(c.a) && coin_rush::hunter_from_name(c.b)),
              "%s touched %s: hunters must pass through each other", c.a.c_str(), c.b.c_str());
    }
    CHECK(caught_tick >= 0, "no hunter caught a player standing still in 120 s");
    if (caught_tick >= 0) {
        std::printf("coin_rush_test: a player standing still was caught after %.1f s (%s)\n",
                    caught_tick / 60.0, model.c_str());
        session.tick();  // The hold lands on the next tick.
        std::vector<glm::vec3> at_catch;
        for (const char* n : names) at_catch.push_back(where(n));
        const std::string model_at_catch = model;
        for (int i = 0; i < 120; ++i) {
            session.tick();
            model = run_model(session, model);
        }
        CHECK(model == model_at_catch, "the run changed after the catch: %s", model.c_str());
        CHECK(field(model, "phase") == "over" && field(model, "best") == field(model, "score"),
              "results: %s", model.c_str());
        for (size_t i = 0; i < 4; ++i) {
            CHECK(glm::distance(where(names[i]), at_catch[i]) < 0.01f,
                  "%s kept moving after the catch", names[i]);
        }
        // Play Again: a fresh run on level 1 with score 0; the best stays.
        const std::string best = field(model, "best");
        model = start_playing(session, model);
        CHECK(field(model, "phase") == "playing" && field(model, "score") == "0" &&
                  field(model, "caught") == "false" && field(model, "best") == best,
              "Play Again: %s", model.c_str());
    }
    session.stop();
}

// The run flow (docs/run-flow.md): the session opens on the start screen with everything held
// (swipes do nothing); Play counts 3-2-1 with everything frozen, then the player is on a central
// spawn and drives; pause freezes everyone and resume goes on; Play from the pause menu starts
// over on a random spawn.
void test_run_flow(const scene::Scene& scene, const std::string& content,
                   const coin_rush::Maze& maze) {
    runtime::Session session;
    session.start(scene, content);
    const auto player = [&]() -> glm::vec3 {
        for (const scene::Entity& e : session.scene().entities) {
            for (const auto& box : e.module_components) {
                if (const auto* r = scene::module_component_as<robots::Robot>(*box)) {
                    if (r->player) return e.transform.position;
                }
            }
        }
        return glm::vec3(-1000.0f);
    };
    const auto node_at = [&](const std::string& id) {
        for (const rails::Node& n : maze.graph.nodes) {
            if (n.id == id) return n.data.position;
        }
        return glm::vec3(1000.0f);
    };
    const auto flat = [](glm::vec3 a, glm::vec3 b) {
        return glm::distance(glm::vec2(a.x, a.z), glm::vec2(b.x, b.z));
    };
    const auto swipe_up = [&]() {
        const float w = 540.0f, h = 1170.0f;
        session.pointer({runtime::PointerEvent::Type::Down, 270.0f, 700.0f, w, h});
        session.pointer({runtime::PointerEvent::Type::Move, 270.0f, 550.0f, w, h});
        session.pointer({runtime::PointerEvent::Type::Up, 270.0f, 400.0f, w, h});
    };
    const auto run_for = [&](int ticks, std::string& model) {
        for (int i = 0; i < ticks; ++i) {
            session.tick();
            model = run_model(session, model);
        }
    };

    std::string model;
    run_for(30, model);
    const glm::vec3 menu_at = player();
    swipe_up();
    run_for(120, model);
    CHECK(field(model, "phase") == "menu", "start: %s", model.c_str());
    CHECK(glm::distance(player(), menu_at) < 0.01f, "the player drove on the start screen");

    session.ui_request("play", "{}");
    run_for(1, model);
    CHECK(field(model, "phase") == "countdown" && field(model, "countdown") == "3",
          "after Play: %s", model.c_str());
    const std::string spawn = field(model, "spawn");
    CHECK(!spawn.empty(), "no spawn: %s", model.c_str());
    swipe_up();
    run_for(60, model);
    CHECK(field(model, "countdown") == "2", "1 s into the countdown: %s", model.c_str());
    const glm::vec3 counting_at = player();
    CHECK(flat(counting_at, node_at(spawn)) < 0.5f, "the player isn't on spawn %s",
          spawn.c_str());
    run_for(120, model);
    CHECK(field(model, "phase") == "playing", "after 3 s: %s", model.c_str());
    CHECK(glm::distance(player(), counting_at) < 0.01f, "the player moved in the countdown");

    swipe_up();
    run_for(60, model);
    CHECK(flat(player(), counting_at) > 1.0f, "the player didn't drive after the countdown");
    session.ui_request("pause", "{}");
    run_for(2, model);
    CHECK(field(model, "phase") == "paused", "after pause: %s", model.c_str());
    const glm::vec3 paused_at = player();
    run_for(120, model);
    CHECK(glm::distance(player(), paused_at) < 0.01f, "the player moved while paused");
    session.ui_request("menu", "{}");  // The pause menu's Menu button.
    run_for(1, model);
    CHECK(field(model, "phase") == "menu", "pause -> menu: %s", model.c_str());

    std::set<std::string> spawns;
    for (int run = 0; run < 8; ++run) {
        session.ui_request("play", "{}");
        run_for(3 * 60 + 1 + run * 7, model);
        CHECK(field(model, "phase") == "playing" && field(model, "score") == "0",
              "run %d: %s", run, model.c_str());
        spawns.insert(field(model, "spawn"));
        session.ui_request("pause", "{}");
        run_for(1, model);
        if (run == 0) {
            session.ui_request("resume", "{}");
            run_for(1, model);
            CHECK(field(model, "phase") == "playing", "resume: %s", model.c_str());
            session.ui_request("pause", "{}");
            run_for(1, model);
        }
    }
    session.stop();
    std::printf("coin_rush_test: 8 runs started on %zu different spawns\n", spawns.size());
    CHECK(spawns.size() >= 2, "every run started on the same spawn");
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
    test_run_with_hunters(*scene, project->content);
    test_hunter_rules();

    std::optional<coin_rush::Maze> maze = coin_rush::load_maze(*scene, project->content, error);
    CHECK(maze.has_value(), "%s", error.c_str());
    if (maze) {
        test_maze_rules(*maze, *scene);
        test_maze_check_catches_dead_end(*maze);
        test_coin_spots(*maze);
        test_levels(*maze);
        test_drive_picks_up_coins(*scene, project->content);
        test_run_flow(*scene, project->content, *maze);
        test_choose_swipe(*maze);
        test_hunters_catch(*maze);
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
