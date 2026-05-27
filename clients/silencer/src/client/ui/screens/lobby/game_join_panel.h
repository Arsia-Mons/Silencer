#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H

// Screen-side lobby GameJoinPanel: three stacked Chrome+Compact buttons
// (Choose Tech / Change Team / Ready) on the upper right pane plus the
// joined-game roster in the tall pane. The Ready button label flips to
// "Waiting..." while the host is still waiting for peers to finish
// downloading the map.
//
// Domain reads and writes come through lobby hooks/providers. Button callbacks
// are captured during BuildUi and drained after Clay declaration.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <functional>
#include <memory>

class Resources;

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::hooks {
struct LobbyGameJoinActions;
}

namespace silencer::client_ui::lobby {

struct GameJoinPanelState {
	std::shared_ptr<silencer::client_ui::hooks::LobbyGameJoinActions> pendingActions = {};
	std::function<void(std::shared_ptr<silencer::client_ui::hooks::LobbyGameJoinActions>)>
		flushActions = {};
	bool actionsQueued = false;
};

void GameJoinPanelInit(GameJoinPanelState & state);

bool GameJoinPanelHandleUiIntent(GameJoinPanelState & state,
                                 const silencer::ui::UiAction & action);

// Emits the upper stepped-pane subtree (Choose Tech / Change Team / Ready
// buttons, stacked vertically). Must be called inside the LobbyRightUpperBox
// CLAY block; emits flex children only (no floating). Caller's BeginFrame
// requirements: ButtonBeginFrame.
void BuildGameJoinUpperTree(GameJoinPanelState & state,
                            Uint16 panelWidth,
                            Resources & resources,
                            silencer::ui::UiInteractionRegistry& interactions);

// Emits the tall stepped-pane subtree (joined-game roster). Must be called
// inside the LobbyRightTallBox CLAY block.
void BuildGameJoinTallTree(GameJoinPanelState & state,
                           Resources & resources,
                           silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
