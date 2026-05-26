#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_TECH_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_TECH_PANEL_H

// Screen-side lobby GameTechPanel: a 4-column tech-choice grid (3 remote-peer
// columns + 1 local column), per-peer name overlays + column separator
// sprites, the "Tech slots left: N" status text, a clickable tech name
// overlay per local row, the centered tech-name + 8 description-line block,
// and a "Back To Teams" chrome button.
//
// Domain reads/writes come through lobby hooks/providers. Right-pane swaps are
// requested through queued callbacks owned by LobbyScreen.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class ScreenContext;

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::hooks {
struct LobbyGameTechActions;
struct LobbyTechItemDetails;
}

namespace silencer::client_ui::lobby {

struct GameTechGridCell {
	bool draw = false;
	bool selected = false;
	Uint8 brightness = 64;
};

struct GameTechGridRow {
	int itemIndex = -1;
	std::array<GameTechGridCell, 4> cells{};
	std::string label;
	Uint8 labelBrightness = 64;
};

struct GameTechPanelState {
	std::shared_ptr<silencer::client_ui::hooks::LobbyGameTechActions> pendingActions = {};
	std::function<void(std::shared_ptr<silencer::client_ui::hooks::LobbyGameTechActions>)>
		flushActions = {};
	std::function<silencer::client_ui::hooks::LobbyTechItemDetails(int itemIndex)>
		techItemDetailsForIndex = {};
	bool actionsQueued = false;

	// Pointer-stable strings (lifetime spans the layout pass via screen
	// ownership). Recomputed each Tick.
	std::string slotsLeftStr;                       // uid 70
	std::string techNameStr;                        // uid 60 — "-ItemName-"
	std::array<std::string, 8> techDescLines{};     // uid 61..68
	std::array<std::string, 3> peerNameStrs{};      // uid 80..82 (non-local peers)
	std::vector<GameTechGridRow> rows;
};

void GameTechPanelInit(GameTechPanelState & state);

// Per-frame pump. Recomputes slots-left and peer names from ScreenContext while
// lobby writes stay behind UseLobby callbacks.
void GameTechPanelTick(GameTechPanelState & state,
                       ScreenContext & ctx);
bool GameTechPanelHandleUiIntent(GameTechPanelState & state,
                                 const silencer::ui::UiAction & action);

// Emits the upper stepped-pane subtree ("Back To Teams" button + 3
// right-aligned peer-name labels). Called inside the LobbyRightUpperBox CLAY
// block; flex children only (no floating).
// BeginFrame requirements: TextBeginFrame, ButtonBeginFrame.
void BuildGameTechUpperTree(GameTechPanelState & state,
                            Uint16 panelWidth,
                            silencer::ui::UiInteractionRegistry& interactions);

// Emits the tall stepped-pane subtree (slots-left text + 4-column tech-choice
// grid + centered tech-name heading + 8 description lines). Called inside
// the LobbyRightTallBox CLAY block; flex children only.
// BeginFrame requirements: TextBeginFrame, ToggleBeginFrame.
void BuildGameTechTallTree(GameTechPanelState & state,
                           silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
