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

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::lobby {

struct GameTechPanelIntent {
	enum class Kind {
		None,
		BackToTeams,
		ToggleTech,
		Handled,
	};
	Kind kind = Kind::None;
	int itemIndex = -1;
};

struct GameTechPanelState {
	int selectedTechItemIndex = -1;
};

void GameTechPanelInit(GameTechPanelState & state);

GameTechPanelIntent GameTechPanelHandleUiIntent(GameTechPanelState & state,
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
