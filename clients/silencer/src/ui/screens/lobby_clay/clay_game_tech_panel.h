#ifndef SILENCER_UI_LOBBY_CLAY_GAME_TECH_PANEL_H
#define SILENCER_UI_LOBBY_CLAY_GAME_TECH_PANEL_H

// Screen-side Clay reimplementation of the lobby GameTechPanel — a 4-column
// tech-choice grid (3 remote-peer columns + 1 local column), per-peer name
// overlays + column separator sprites, the "Tech slots left: N" status text,
// a clickable tech name overlay per local row, the centered tech-name + 8
// description-line block, and a "Back To Teams" chrome button.
//
// Domain glue (World::SetTech, Config save, ShowGameJoin) lives in the
// screen-side GameTechPanelTick. Primitives stay screen-agnostic.

#include "shared.h"

#include <array>
#include <string>

class World;
class Resources;
class ScreenContext;
class LobbyClayScreen;

namespace silencer::ui::lobby_clay {

struct GameTechPanelState {
	// Per-frame click flags. Set by Clay onClick adapters; consumed by Tick.
	bool backClicked = false;
	// -1 = no click this frame; otherwise the buyableitems[idx] index.
	int  toggleClickedItemIndex = -1;
	int  descClickedItemIndex   = -1;

	// Pointer-stable strings (lifetime spans the layout pass via screen
	// ownership). Recomputed each Tick.
	std::string slotsLeftStr;                       // uid 70
	std::string techNameStr;                        // uid 60 — "-ItemName-"
	std::array<std::string, 8> techDescLines{};     // uid 61..68
	std::array<std::string, 3> peerNameStrs{};      // uid 80..82 (non-local peers)
};

void GameTechPanelInit(GameTechPanelState & state);

// Per-frame pump. Recomputes slots-left, peer names, and consumes per-frame
// click flags (toggle a tech bit, swap description, exit on Back).
void GameTechPanelTick(GameTechPanelState & state,
                       World & world,
                       ScreenContext & ctx,
                       LobbyClayScreen & owner);

// Emits the Clay subtree. Must be called inside an open Clay layout pass
// AFTER BankTextBeginFrame() + BankButtonBeginFrame() + ToggleBeginFrame().
void BuildGameTechPanelTree(GameTechPanelState & state,
                            World & world,
                            Resources & resources,
                            LobbyClayScreen & owner);

}  // namespace silencer::ui::lobby_clay

#endif
