#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_TECH_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_TECH_PANEL_H

// Screen-side lobby GameTechPanel state and domain glue. The cppx lobby view
// owns retained composition; this file owns World::SetTech, Config persistence,
// peer-name snapshots, and the selected-tech description snapshot.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <array>
#include <string>
#include <vector>

class World;
class ScreenContext;
class LobbyScreen;

namespace silencer::client_ui::lobby {

struct GameTechRow {
	bool visible = false;
	bool selected = false;
	std::string label;
};

struct GameTechPanelState {
	// Per-frame click flags. Set by typed widget intents; consumed by Tick.
	bool backClicked = false;
	// -1 = no click this frame; otherwise the buyableitems[idx] index.
	int  toggleClickedItemIndex = -1;
	int  descClickedItemIndex   = -1;

// Pointer-stable strings for retained view props. Recomputed each Tick.
	std::string slotsLeftStr;                       // uid 70
	std::string techNameStr;                        // uid 60 — "-ItemName-"
	std::array<std::string, 8> techDescLines{};     // uid 61..68
	std::array<std::string, 3> peerNameStrs{};      // uid 80..82 (non-local peers)
	std::vector<GameTechRow> techRows;
};

void GameTechPanelInit(GameTechPanelState & state);

// Per-frame pump. Recomputes slots-left, peer names, and consumes per-frame
// click flags (toggle a tech bit, swap description, exit on Back).
void GameTechPanelTick(GameTechPanelState & state,
                       World & world,
                       ScreenContext & ctx,
                       LobbyScreen & owner);
bool GameTechPanelHandleUiIntent(GameTechPanelState & state,
                                 const silencer::ui::UiAction & action);
void GameTechPanelRequestBack(GameTechPanelState & state);
void GameTechPanelPreviewItem(GameTechPanelState & state, int index);
void GameTechPanelToggleItem(GameTechPanelState & state, int index);

}  // namespace silencer::client_ui::lobby

#endif
