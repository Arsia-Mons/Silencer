#pragma once

#include <string>

namespace silencer {
namespace client_ui {

enum class ScreenId {
	MainMenu,
	LobbyConnect,
	Lobby,
	Options,
	Update,
	MissionSummary,
};

struct ClientUiState {
	ScreenId activeScreen = ScreenId::MainMenu;
	std::string focusedElementId;
};

}  // namespace client_ui
}  // namespace silencer
