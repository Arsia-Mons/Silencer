#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_TECH_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_TECH_PANEL_H

// Screen-side lobby GameTechPanel: a 4-column tech-choice grid (3 remote-peer
// columns + 1 local column), per-peer name overlays + column separator
// sprites, the "Tech slots left: N" status text, a clickable tech name
// overlay per local row, the centered tech-name + 8 description-line block,
// and a "Back To Teams" chrome button.
//
// Domain glue (World::SetTech, Config save, ShowGameJoin) lives in the
// screen-side GameTechPanelTick. Primitives stay screen-agnostic.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <array>
#include <string>

class World;
class Resources;
class ScreenContext;
class LobbyScreen;

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::lobby {

struct GameTechPanelState {
	// Per-frame click flags. Set by typed widget intents; consumed by Tick.
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
                       LobbyScreen & owner);
bool GameTechPanelHandleUiIntent(GameTechPanelState & state,
                                 const silencer::ui::UiAction & action);

// Emits the upper stepped-pane subtree ("Back To Teams" button + 3
// right-aligned peer-name labels). Called inside the LobbyRightUpperBox CLAY
// block; flex children only (no floating).
// BeginFrame requirements: TextBeginFrame, ButtonBeginFrame.
void BuildGameTechUpperTree(GameTechPanelState & state,
                            World & world,
                            Resources & resources,
                            LobbyScreen & owner,
                            silencer::ui::UiInteractionRegistry& interactions);

// Emits the tall stepped-pane subtree (slots-left text + 4-column tech-choice
// grid + centered tech-name heading + 8 description lines). Called inside
// the LobbyRightTallBox CLAY block; flex children only.
// BeginFrame requirements: TextBeginFrame, ToggleBeginFrame.
void BuildGameTechTallTree(GameTechPanelState & state,
                           World & world,
                           Resources & resources,
                           LobbyScreen & owner,
                           silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
