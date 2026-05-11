#ifndef SILENCER_UI_LOBBY_CLAY_GAME_JOIN_PANEL_H
#define SILENCER_UI_LOBBY_CLAY_GAME_JOIN_PANEL_H

// Screen-side Clay reimplementation of the lobby GameJoinPanel — three
// stacked B156x21 chrome buttons (Choose Tech / Change Team / Ready) on the
// right pane. The Ready button label flips to "Waiting..." while the host is
// still waiting for peers to finish downloading the map.
//
// Domain glue (SendReady, ChangeTeam, ShowGameTech) lives in the screen-side
// GameJoinPanelTick. Primitives stay screen-agnostic.

#include "shared.h"

#include <string>

class World;
class Resources;
class ScreenContext;
class LobbyClayScreen;

namespace silencer::ui::lobby_clay {

struct GameJoinPanelState {
	// Per-frame click flags. Set by Clay onClick adapters; consumed once
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
                       LobbyClayScreen & owner);

// Emits the Clay subtree. Must be called inside an open Clay layout pass
// AFTER BankTextBeginFrame() + BankButtonBeginFrame().
void BuildGameJoinPanelTree(GameJoinPanelState & state,
                            Resources & resources);

}  // namespace silencer::ui::lobby_clay

#endif
