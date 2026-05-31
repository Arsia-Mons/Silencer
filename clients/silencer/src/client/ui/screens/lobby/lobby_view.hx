#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

class World;

namespace silencer::client_ui::lobby {

struct CharacterPanelState;
struct ChatPanelState;
struct GameSelectPanelState;
struct GameCreatePanelState;
struct GameJoinPanelState;
struct GameTechPanelState;

struct LobbyContextValue {
	const char * version = "";
	const char * map_name = "";
	World * world = nullptr;
	CharacterPanelState * character = nullptr;
	ChatPanelState * chat = nullptr;
	GameSelectPanelState * game_select = nullptr;
	GameCreatePanelState * game_create = nullptr;
	GameJoinPanelState * game_join = nullptr;
	GameTechPanelState * game_tech = nullptr;
	bool game_create_active = false;
	bool game_join_active = false;
	bool game_tech_active = false;
	bool * go_back_clicked = nullptr;
	bool * chat_send_clicked = nullptr;
};

extern ::ReactContext LobbyContext;

const LobbyContextValue& UseLobby();

struct LobbyScreenViewProps {
	const char * key = nullptr;
	const LobbyContextValue * value = nullptr;
};

::ui::UiElement LobbyScreenView(const LobbyScreenViewProps& props);

}  // namespace silencer::client_ui::lobby
