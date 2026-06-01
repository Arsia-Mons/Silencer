#pragma once

#include <functional>

namespace silencer {
namespace client_ui {

enum class ScreenDestination {
	MainMenu,
	SinglePlayerGame,
	LobbyConnect,
	Lobby,
	CreateCharacter,
	Updating,
	MissionSummary,
	Options,
	OptionsControls,
	OptionsDisplay,
	OptionsAudio,
};

struct GameNavigation {
	std::function<void(ScreenDestination)> go_to = {};
	std::function<void()> go_back = {};
	std::function<void()> request_quit = {};
};

GameNavigation use_game_navigation();

}  // namespace client_ui
}  // namespace silencer
