#pragma once

// Coin Rush's game code. The engine calls register_game() once at startup (its generated game
// list, cmake/Game.cmake); what it registers works only in this project (document/game.hpp).

#include "document/document.hpp"

namespace coin_rush {

void register_game(doc::Registry& registry);

}  // namespace coin_rush
