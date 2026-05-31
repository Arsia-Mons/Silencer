#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H

// Screen-side lobby GameJoinPanel state and domain glue. The cppx lobby view
// owns retained composition; this file owns SendReady, ChangeTeam, ShowGameTech,
// and the joined-game roster snapshot.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <string>
#include <vector>

class World;
class ScreenContext;
class LobbyScreen;

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

}  // namespace silencer::client_ui::lobby

#endif
