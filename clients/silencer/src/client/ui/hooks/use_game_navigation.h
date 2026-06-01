#pragma once

#include <SDL3/SDL_stdinc.h>

#include <functional>

namespace silencer {
namespace client_ui {

struct GameNavigation {
	std::function<void(Uint8)> go_to_state = {};
	std::function<void()> go_back = {};
	std::function<void()> request_quit = {};
};

GameNavigation use_game_navigation();

}  // namespace client_ui
}  // namespace silencer
