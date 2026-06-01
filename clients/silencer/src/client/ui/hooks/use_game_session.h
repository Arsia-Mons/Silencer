#pragma once

#include <functional>

namespace silencer {
namespace client_ui {

struct GameSessionActions {
	std::function<void()> start_tutorial = {};
	std::function<void()> request_quit = {};
};

GameSessionActions use_game_session();

}  // namespace client_ui
}  // namespace silencer
