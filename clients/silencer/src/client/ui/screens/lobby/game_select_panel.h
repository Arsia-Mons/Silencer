#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H

// Screen-side lobby GameSelectPanel: the always-on right-side games list
// surface (active when no Create/Join/Tech panel is up). Composes ScrollList
// + Text + Button primitives and owns the per-frame info-block
// strings + the Join/Spectate/Create button click flags. Domain mutations go
// through use_lobby(); primitives stay screen-agnostic.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <string>
#include <vector>

namespace silencer::ui {
class UiInteractionRegistry;
}

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

	// Per-frame click flags. Set by typed widget intents; consumed
	// once by GameSelectPanelTick on the next frame.
	bool   joinClicked     = false;
	bool   spectateClicked = false;
	bool   createClicked   = false;

	// Per-row click flag — the row that was last pressed. -1 = none.
	int    rowClickedIndex = -1;

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

struct GameSelectPanelTickResult {
	bool show_create = false;
};

// Per-frame pump:
//   - Rebuilds `rows` from LobbyBrowserModel whenever the game list is dirty.
//   - Recomputes infoName/infoMap/.../joinVisible/spectateVisible from the
//     selected game (or clears them when no game is selected).
//   - Consumes joinClicked / spectateClicked / createClicked flags by asking
//     the lobby model to run Join/Spectate/Create transitions.
GameSelectPanelTickResult GameSelectPanelTick(GameSelectPanelState & state,
                                              LobbyModel & lobby);
bool GameSelectPanelHandleUiIntent(GameSelectPanelState & state,
                                   const silencer::ui::UiAction & action);

// Emits the upper stepped-pane subtree (Create Game button). Must be called
// inside the LobbyRightUpperBox CLAY block; emits flex children only (no
// floating).
// BeginFrame requirements: ButtonBeginFrame.
void BuildGameSelectUpperTree(GameSelectPanelState & state,
                              Uint16 panelWidth,
                              silencer::ui::UiInteractionRegistry& interactions);

// Emits the tall stepped-pane subtree ("Active Games" header + games list +
// info-block + Spectate/Join buttons). Must be called inside the
// LobbyRightTallBox CLAY block; emits flex children only.
// BeginFrame requirements: TextBeginFrame, ButtonBeginFrame,
// ScrollListBeginFrame.
void BuildGameSelectTallTree(GameSelectPanelState & state,
                             Uint16 panelWidth,
                             Uint16 panelHeight,
                             silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
