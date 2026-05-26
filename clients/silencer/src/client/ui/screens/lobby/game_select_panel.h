#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_SELECT_PANEL_H

// Screen-side lobby GameSelectPanel: the always-on right-side games list
// surface (active when no Create/Join/Tech panel is up). Composes ScrollList
// + Text + Button primitives and owns the per-frame info-block
// strings + the Join/Spectate/Create button click flags. Domain glue
// (JoinGame / SpectateGame / level checks / password modal) lives here in
// the screen-side panel; parent-owned panel swaps stay in the owning screen.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <string>
#include <vector>

class Resources;
class ScreenContext;

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::lobby {

struct GameSelectPanelState {
	// Snapshot of the games list. Rebuilt when ScreenContext consumes a
	// pending lobby game-list refresh. Held in screen-side storage so the
	// layout pass can hold pointers into the std::strings safely.
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
	// game). Pointer-stable so the layout pass can hold raw c_str()s.
	std::string infoName;
	std::string infoMap;
	std::string infoSecurity;
	std::string infoCreator;
	std::string infoLimits;

	// Cached visibility for Join/Spectate (recomputed each Tick).
	bool joinVisible     = false;
	bool spectateVisible = false;
};

// One-time init. Clears state. The first Tick that consumes a pending
// ScreenContext lobby game-list refresh will populate copied id/name rows.
void GameSelectPanelInit(GameSelectPanelState & state);

// Per-frame pump:
//   - Rebuilds `rows` from copied ScreenContext lobby game-list rows when a
//     pending refresh is consumed.
//   - Recomputes infoName/infoMap/.../joinVisible/spectateVisible from the
//     selected game (or clears them when no game is selected).
//   - Consumes joinClicked / spectateClicked flags — runs the legacy
//     GameSelectPanel::Tick's Join/Spectate flows (level checks, password
//     modal, JoinGame/SpectateGame).
//   - Leaves createClicked for the owning screen to consume because
//     swapping the right-side panel is parent-owned navigation.
void GameSelectPanelTick(GameSelectPanelState & state,
                         ScreenContext & ctx);
bool GameSelectPanelHandleUiIntent(GameSelectPanelState & state,
                                   const silencer::ui::UiAction & action);

// Emits the upper stepped-pane subtree (Create Game button). Must be called
// inside the LobbyRightUpperBox CLAY block; emits flex children only (no
// floating).
// BeginFrame requirements: ButtonBeginFrame.
void BuildGameSelectUpperTree(GameSelectPanelState & state,
                              Uint16 panelWidth,
                              Resources & resources,
                              silencer::ui::UiInteractionRegistry& interactions);

// Emits the tall stepped-pane subtree ("Active Games" header + games list +
// info-block + Spectate/Join buttons). Must be called inside the
// LobbyRightTallBox CLAY block; emits flex children only.
// BeginFrame requirements: TextBeginFrame, ButtonBeginFrame,
// ScrollListBeginFrame.
void BuildGameSelectTallTree(GameSelectPanelState & state,
                             Uint16 panelWidth,
                             Uint16 panelHeight,
                             Resources & resources,
                             silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
