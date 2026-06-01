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
		CharacterPanelState * character = nullptr;
		ChatPanelState * chat = nullptr;
		GameSelectPanelState * game_select = nullptr;
		GameCreatePanelState * game_create = nullptr;
		GameJoinPanelState * game_join = nullptr;
		GameTechPanelState * game_tech = nullptr;
		bool game_create_active = false;
		bool game_join_active = false;
		bool game_tech_active = false;
	};

	struct Actions {
		std::function<void()> change_agent = {};
		std::function<void(const std::string&)> set_chat_text = {};
		std::function<void()> send_chat = {};
		std::function<void(int)> select_game = {};
		std::function<void(int)> scroll_games = {};
		std::function<void()> create_game = {};
		std::function<void()> join_game = {};
		std::function<void()> spectate_game = {};
		std::function<void(int)> select_create_map = {};
		std::function<void(int)> scroll_create_maps = {};
		std::function<void()> cycle_create_security = {};
		std::function<void()> toggle_create_spectatable = {};
		std::function<void()> submit_create_game = {};
		std::function<void(const std::string&)> set_create_name = {};
		std::function<void(const std::string&)> set_create_password = {};
		std::function<void(const std::string&)> set_create_min_level = {};
		std::function<void(const std::string&)> set_create_max_level = {};
		std::function<void(const std::string&)> set_create_max_players = {};
		std::function<void(const std::string&)> set_create_max_teams = {};
		std::function<void()> choose_tech = {};
		std::function<void()> change_team = {};
		std::function<void()> ready_game = {};
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

struct LobbyFrameProps {
	const char * key = nullptr;
};

::ui::UiElement LobbyFrame(const LobbyFrameProps& props);

struct LobbyScreenViewProps {
	const char * key = nullptr;
	const LobbyContextValue * value = nullptr;
	const LobbyNavigation * navigation = nullptr;
};

::ui::UiElement LobbyScreenView(const LobbyScreenViewProps& props);

}  // namespace silencer::client_ui::lobby
