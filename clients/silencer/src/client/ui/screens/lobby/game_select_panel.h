#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H

// Screen-side lobby GameSelectPanel: the always-on right-side games list
// surface (active when no Create/Join/Tech panel is up). Owns the retained
// frame's per-frame row snapshot and info strings. Domain mutations go through
// use_lobby().

#include "shared.h"

#include <string>
#include <vector>

namespace silencer::client_ui {
class LobbyModel;
}

namespace silencer::client_ui::lobby {

struct GameSelectPanelState {
	// Snapshot of the games list. Rebuilt from LobbyBrowserModel whenever its
	// dirty flag says the lobby game list changed. Held in screen-side storage
	// so the layout pass can hold pointers into the std::strings safely.
	struct Row {
		std::string name;
		Uint32      gameid = 0;
	};
	std::vector<Row> rows;
	int    selectedIndex = -1;  // -1 = no selection.
	Uint16 scrollPos     = 0;

	// Cached info-block strings (recomputed each Tick from the selected
	// game). Static-lifetime so the layout pass can hold raw c_str()s.
	std::string infoName;
	std::string infoMap;
	std::string infoSecurity;
	std::string infoCreator;
	std::string infoLimits;

	// Cached visibility for Join/Spectate (recomputed each Tick).
	bool joinVisible     = false;
	bool spectateVisible = false;
};

// One-time init. Clears state. The first Tick that observes a dirty
// LobbyBrowserModel game list will populate rows.
void GameSelectPanelInit(GameSelectPanelState & state);

// Per-frame pump:
//   - Rebuilds `rows` from LobbyBrowserModel whenever the game list is dirty.
//   - Recomputes infoName/infoMap/.../joinVisible/spectateVisible from the
//     selected game (or clears them when no game is selected).
void GameSelectPanelTick(GameSelectPanelState & state,
                         LobbyModel & lobby);

void GameSelectPanelSelect(GameSelectPanelState & state,
                           int index);
Uint32 GameSelectPanelSelectedGameId(const GameSelectPanelState & state);

}  // namespace silencer::client_ui::lobby

#endif
