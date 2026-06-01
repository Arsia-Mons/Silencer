#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>
#include <string>

namespace silencer::client_ui::lobby {

struct LobbyChat;
struct LobbyCharacter;
struct LobbyGameSelect;
struct LobbyGameCreate;
struct LobbyGameJoin;
struct LobbyGameTech;

struct LobbyChrome {
	const char * version = "";
	const char * map_name = "";
};

const LobbyChrome& UseLobbyChrome();

struct LobbySurface {
	bool game_create_active = false;
	bool game_join_active = false;
	bool game_tech_active = false;
};

const LobbySurface& UseLobbySurface();

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
	const LobbyChrome * chrome = nullptr;
	const LobbySurface * surface = nullptr;
	const LobbyNavigation * navigation = nullptr;
	const LobbyChat * chat = nullptr;
	const LobbyCharacter * character = nullptr;
	const LobbyGameSelect * game_select = nullptr;
	const LobbyGameCreate * game_create = nullptr;
	const LobbyGameJoin * game_join = nullptr;
	const LobbyGameTech * game_tech = nullptr;
};

::ui::UiElement LobbyScreenView(const LobbyScreenViewProps& props);

}  // namespace silencer::client_ui::lobby
