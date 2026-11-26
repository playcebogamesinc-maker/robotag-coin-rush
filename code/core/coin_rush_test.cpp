// coin_rush_test: Coin Rush's start scene loads and plays two seconds with the game's code.
// Checks: the run publishes the HUD's "run" model with a score of 0. Exit code 0 = pass
// (ctest, label game:coin_rush).

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

#include "all_modules.hpp"
#include "core/coin_rush.hpp"
#include "document/game.hpp"
#include "project/project.hpp"
#include "runtime/session.hpp"
#include "scene/scene.hpp"

int main() {
    doc::Registry registry;
    modules::register_all_modules(registry);
    {
        const doc::GameScope scope("coin_rush");  // What the engine's game list does.
        coin_rush::register_game(registry);
    }

    std::string error;
    std::optional<project::OpenProject> project = project::load_project(RT_GAME_DIR, error);
    if (project && project::activate_game(*project, error)) {
        std::optional<scene::Scene> scene =
            scene::load_scene(project->content_path(project->info.start_scene), error);
        if (scene) {
            runtime::Session session;
            session.start(*scene, project->content);
            for (int i = 0; i < 120; ++i) session.tick();
            bool score_shown = false;
            for (const auto& [name, json] : session.take_ui_models()) {
                if (name == "run" && json == "{\"score\":0}") score_shown = true;
            }
            session.stop();
            if (!score_shown) {
                std::printf("FAILED: the run didn't publish the \"run\" model with score 0\n");
                return EXIT_FAILURE;
            }
            std::printf("coin_rush_test: all passed\n");
            return EXIT_SUCCESS;
        }
    }
    std::printf("FAILED: %s\n", error.c_str());
    return EXIT_FAILURE;
}
