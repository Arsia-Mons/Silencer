#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H

// Screen-side lobby GameJoinPanel: three stacked B156x21 chrome buttons
// (Choose Tech / Change Team / Ready) on the right pane. The Ready button
// label flips to "Waiting..." while the host is still waiting for peers to
// finish downloading the map.
//
// Domain glue (SendReady, ChangeTeam, ShowGameTech) lives in the screen-side
// GameJoinPanelTick. Primitives stay screen-agnostic.

#include "shared.h"

#include <string>

class World;
class Resources;
class ScreenContext;
class LobbyScreen;

namespace silencer::client_ui::lobby {

struct GameJoinPanelState {
	// Per-frame click flags. Set by widget onClick adapters; consumed once
	// by GameJoinPanelTick on the next frame.
	bool readyClicked = false;
	bool teamClicked  = false;
	bool techClicked  = false;

	// Cached Ready-button label — recomputed each Tick from
	// world.gameplaystate / localpeer.ishost / AllPeersDownloadedMap.
	// Pointer-stable across Build calls because it's std::string-owned
	// on the screen.
	std::string readyLabel = "Ready";
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

// Emits the upper-pane subtree (Choose Tech / Change Team / Ready buttons,
// stacked vertically). Must be called inside the LobbyRightUpperBox CLAY
// block; emits flex children only (no floating). Caller's BeginFrame
// requirements: BankButtonBeginFrame.
void BuildGameJoinUpperTree(GameJoinPanelState & state,
                            Resources & resources);

// Emits the tall-pane subtree (currently empty for GameJoin — the variant
// has no tall-area content). Must be called inside the LobbyRightTallBox
// CLAY block.
void BuildGameJoinTallTree(GameJoinPanelState & state,
                           Resources & resources);

}  // namespace silencer::client_ui::lobby

#endif
