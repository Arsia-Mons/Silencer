#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H

// Screen-side lobby GameJoinPanel: three stacked Chrome+Compact buttons
// (Choose Tech / Change Team / Ready) on the upper right pane plus the
// joined-game roster in the tall pane. The Ready button label flips to
// "Waiting..." while the host is still waiting for peers to finish
// downloading the map.
//
// Domain glue (SendReady, ChangeTeam, ShowGameTech) lives in the screen-side
// GameJoinPanelTick. Primitives stay screen-agnostic.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <string>
#include <vector>

class World;
class Resources;
class ScreenContext;
class LobbyScreen;

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::lobby {

struct GameJoinRosterRow {
	bool ready = false;
	Uint8 agency = 0;
	Uint8 teamNumber = 0;
	Uint8 peerSlot = 0;
	bool drawEmblem = false;
	std::string name;
	std::string level;
};

struct GameJoinPanelState {
	// Per-frame click flags. Set by typed widget intents; consumed once
	// by GameJoinPanelTick on the next frame.
	bool readyClicked = false;
	bool teamClicked  = false;
	bool techClicked  = false;

	// Cached Ready-button label — recomputed each Tick from
	// world.gameplaystate / localpeer.ishost / AllPeersDownloadedMap.
	// Pointer-stable across Build calls because it's std::string-owned
	// on the screen.
	std::string readyLabel = "Ready";

	// Joined-game roster shown in the tall pane. Rebuilt every Tick from
	// the connected world's current team/peer state.
	std::vector<GameJoinRosterRow> rosterRows;
};

void GameJoinPanelInit(GameJoinPanelState & state);

// Per-frame pump. Recomputes the Ready-button label (legacy
// `if(world.gameplaystate == INLOBBY) ...` block) and consumes the click
// flags — SendReady on Ready, ChangeTeam on Change Team, owner.ShowGameTech
// on Choose Tech.
void GameJoinPanelTick(GameJoinPanelState & state,
                       World & world,
                       ScreenContext & ctx,
                       LobbyScreen & owner);
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
