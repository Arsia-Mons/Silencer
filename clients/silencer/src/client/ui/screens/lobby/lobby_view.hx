#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>
#include <string>

namespace silencer::client_ui::lobby {

struct CharacterPanelState;
struct ChatPanelState;
struct GameSelectPanelState;
struct GameCreatePanelState;
struct GameJoinPanelState;
struct GameTechPanelState;

struct LobbyContextValue {
	struct State {
		const char * version = "";
		const char * map_name = "";
		GameTechPanelState * game_tech = nullptr;
		bool game_create_active = false;
		bool game_join_active = false;
		bool game_tech_active = false;
	};

	struct Actions {
		std::function<void()> back_to_team = {};
		std::function<void(int)> preview_tech = {};
		std::function<void(int)> toggle_tech = {};
	};

	State state = {};
	Actions actions = {};
};

const LobbyContextValue& UseLobby();

struct LobbyNavigation {
	std::function<void()> go_back = {};
};

const LobbyNavigation& UseLobbyNavigation();

struct LobbyChat {
	ChatPanelState * state = nullptr;
	std::function<void(const std::string&)> set_text = {};
	std::function<void()> send = {};
};

const LobbyChat& UseLobbyChat();

struct LobbyCharacter {
	CharacterPanelState * state = nullptr;
	std::function<void()> change_agent = {};
};

const LobbyCharacter& UseLobbyCharacter();

struct LobbyGameSelect {
	GameSelectPanelState * state = nullptr;
	std::function<void(int)> select = {};
	std::function<void(int)> scroll = {};
	std::function<void()> create = {};
	std::function<void()> join = {};
	std::function<void()> spectate = {};
};

const LobbyGameSelect& UseLobbyGameSelect();

struct LobbyGameCreate {
	GameCreatePanelState * state = nullptr;
	std::function<void(int)> select_map = {};
	std::function<void(int)> scroll_maps = {};
	std::function<void()> cycle_security = {};
	std::function<void()> toggle_spectatable = {};
	std::function<void()> submit = {};
	std::function<void(const std::string&)> set_name = {};
	std::function<void(const std::string&)> set_password = {};
	std::function<void(const std::string&)> set_min_level = {};
	std::function<void(const std::string&)> set_max_level = {};
	std::function<void(const std::string&)> set_max_players = {};
	std::function<void(const std::string&)> set_max_teams = {};
};

const LobbyGameCreate& UseLobbyGameCreate();

struct LobbyGameJoin {
	GameJoinPanelState * state = nullptr;
	std::function<void()> choose_tech = {};
	std::function<void()> change_team = {};
	std::function<void()> ready = {};
};

const LobbyGameJoin& UseLobbyGameJoin();

struct LobbyFrameProps {
	const char * key = nullptr;
};

::ui::UiElement LobbyFrame(const LobbyFrameProps& props);

struct LobbyScreenViewProps {
	const char * key = nullptr;
	const LobbyContextValue * value = nullptr;
	const LobbyNavigation * navigation = nullptr;
	const LobbyChat * chat = nullptr;
	const LobbyCharacter * character = nullptr;
	const LobbyGameSelect * game_select = nullptr;
	const LobbyGameCreate * game_create = nullptr;
	const LobbyGameJoin * game_join = nullptr;
};

::ui::UiElement LobbyScreenView(const LobbyScreenViewProps& props);

}  // namespace silencer::client_ui::lobby
