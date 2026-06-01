#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_TECH_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_TECH_PANEL_H

// Screen-side lobby GameTechPanel: a 4-column tech-choice grid (3 remote-peer
// columns + 1 local column), retained per-peer name labels in the upper pane,
// column separator sprites, the "Tech slots left: N" status text, a clickable
// tech name overlay per local row, the centered tech-name + 8 description-line
// block, and a retained "Back To Teams" chrome button.
//
// Domain mutations go through use_lobby(); primitives stay screen-agnostic.

#include "shared.h"
#include <array>
#include <string>
#include <vector>

namespace silencer::client_ui {
class LobbyModel;
}

namespace silencer::client_ui::lobby {

struct GameTechGridCellState {
	bool draw = false;
	bool local = false;
	bool selected = false;
	Uint8 brightness = 64;
};

struct GameTechGridRowState {
	int itemIndex = -1;
	std::string label;
	Uint8 labelBrightness = 64;
	std::array<GameTechGridCellState, 4> cells{};
};

struct GameTechGridState {
	bool visible = false;
	bool localLabelsVisible = false;
	std::vector<GameTechGridRowState> rows;
};

struct GameTechPanelState {
	// Pointer-stable strings (lifetime spans the layout pass via screen
	// ownership). Recomputed each Tick.
	std::string slotsLeftStr;                       // uid 70
	std::string techNameStr;                        // uid 60 — "-ItemName-"
	std::array<std::string, 8> techDescLines{};     // uid 61..68
	std::array<std::string, 3> peerNameStrs{};      // uid 80..82 (non-local peers)
	GameTechGridState grid;
};

void GameTechPanelInit(GameTechPanelState & state);

// Per-frame read-only refresh of slots-left, peer names, and the tech grid.
void GameTechPanelTick(GameTechPanelState & state,
                       LobbyModel & lobby);
void GameTechPanelDescribe(GameTechPanelState & state,
                           const LobbyModel & lobby,
                           int item_index);

}  // namespace silencer::client_ui::lobby

#endif
