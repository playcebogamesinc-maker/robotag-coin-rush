# Coin Rush's code: its libraries, rt_add_game(), tests. Skill: write-game-code.
#
# RT_GAME_DIR = this project's folder, RT_GAME_ID = its id (cmake/Game.cmake).

# core: game logic (components, systems, documents). No graphics: it also runs on a headless
# server. Add render/ and editor/ libraries when the game needs them (RENDER / EDITOR below).
add_library(coin_rush_core STATIC
    core/coin_rush.hpp
    core/coin_rush.cpp
)
target_include_directories(coin_rush_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})  # "core/..."
# The game's own components and documents: write schemas/coin_rush.schema, then
#   rt_add_schema(coin_rush_core PUBLIC ${RT_GAME_DIR}/schemas/coin_rush.schema)
target_link_libraries(coin_rush_core PUBLIC rt_runtime rt_document rt_scene)
if(MSVC)
    target_compile_options(coin_rush_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(coin_rush_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

rt_add_game(coin_rush CORE coin_rush_core)

if(NOT ANDROID)
    add_executable(coin_rush_test core/coin_rush_test.cpp)
    target_link_libraries(coin_rush_test PRIVATE coin_rush_core rt_project rt_modules)
    target_compile_definitions(coin_rush_test PRIVATE RT_GAME_DIR="${RT_GAME_DIR}")
    rt_add_test(coin_rush_test LABELS game game:coin_rush)
endif()
