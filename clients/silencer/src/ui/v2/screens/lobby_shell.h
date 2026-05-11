#ifndef SILENCER_UI_V2_SCREENS_LOBBY_SHELL_H
#define SILENCER_UI_V2_SCREENS_LOBBY_SHELL_H

#include "shared.h"

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct LobbyHandlers {
	std::function<void()> on_go_back;
};

// Engine-supplied dynamic state for the lobby chrome + embedded panels.
// Kept separate from LobbyHandlers because handlers are pure callbacks
// while these are read on every Build.
struct LobbyState {
	// Currently-selected agency (Team::NOXIS..Team::BLACKROSE = 0..4),
	// drives which CharacterPanel toggle renders at full brightness.
	Uint8 selected_agency = 0;
};

// Lobby chrome plus the character panel. Remaining panels (chat /
// right-side game lists) land in P11+ and get composed in here as they
// do.
Node BuildLobby(const Context & ctx, const LobbyHandlers & handlers = {},
                const LobbyState & state = {});

}  // namespace v2
}  // namespace ui

#endif
