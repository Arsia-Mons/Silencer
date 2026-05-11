#ifndef SILENCER_UI_V2_SCREENS_LOBBY_SHELL_H
#define SILENCER_UI_V2_SCREENS_LOBBY_SHELL_H

#include "shared.h"
#include "lobby_create.h"
#include "lobby_join.h"
#include "lobby_select.h"
#include "lobby_tech.h"

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct LobbyHandlers {
	std::function<void()> on_go_back;
	GameCreateHandlers game_create;
	GameJoinHandlers game_join;
	GameSelectHandlers game_select;
	GameTechHandlers game_tech;
};

// Which right-side panel the lobby is currently displaying. Mirrors the
// legacy LobbyScreen unique_ptr<> swap (gameSelect / gameCreate /
// gameJoin / gameTech) — `None` is the chrome-only state used by the
// initial "lobby" preview before any panel ports landed.
enum class LobbyActivePanel : Uint8 {
	None,
	GameCreate,
	GameJoin,
	GameSelect,
	GameTech,
};

// Engine-supplied dynamic state for the lobby chrome + embedded panels.
// Kept separate from LobbyHandlers because handlers are pure callbacks
// while these are read on every Build.
struct LobbyState {
	// Currently-selected agency (Team::NOXIS..Team::BLACKROSE = 0..4),
	// drives which CharacterPanel toggle renders at full brightness.
	Uint8 selected_agency = 0;
	// Which right-side panel is currently active (drives which Build
	// helper gets composed in BuildLobby + whether the chat caret renders).
	LobbyActivePanel active_panel = LobbyActivePanel::None;
	// Per-panel state. Only the field matching `active_panel` is read.
	GameCreateState game_create;
	GameJoinState game_join;
	GameSelectState game_select;
	GameTechState game_tech;
};

// Lobby chrome plus the character panel. Remaining panels (chat /
// right-side game lists) land in P11+ and get composed in here as they
// do.
Node BuildLobby(const Context & ctx, const LobbyHandlers & handlers = {},
                const LobbyState & state = {});

}  // namespace v2
}  // namespace ui

#endif
