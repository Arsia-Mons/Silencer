#ifndef SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_GAME_JOIN_PANEL_H

// Screen-side lobby GameJoinPanel: retained upper actions
// (Choose Tech / Change Team / Ready) plus the retained joined-game roster.
// The Ready button label flips to "Waiting..." while the host is still waiting
// for peers to finish downloading the map.
//
// Domain mutations go through use_lobby(); primitives stay screen-agnostic.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <string>
#include <vector>

namespace silencer::client_ui {
class LobbyModel;
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

	// Cached Ready-button label — recomputed each Tick from the lobby
	// pregame model.
	// Pointer-stable across Build calls because it's std::string-owned
	// on the screen.
	std::string readyLabel = "Ready";

	// Joined-game roster shown in the tall pane. Rebuilt every Tick from
	// the lobby pregame model.
	std::vector<GameJoinRosterRow> rosterRows;
};

void GameJoinPanelInit(GameJoinPanelState & state);

struct GameJoinPanelTickResult {
	bool show_tech = false;
};

// Per-frame pump. Recomputes the Ready-button label (legacy
// `if(world.gameplaystate == INLOBBY) ...` block) and consumes the click
// flags through the lobby model.
GameJoinPanelTickResult GameJoinPanelTick(GameJoinPanelState & state,
                                          LobbyModel & lobby);
bool GameJoinPanelHandleUiIntent(GameJoinPanelState & state,
                                 const silencer::ui::UiAction & action);

}  // namespace silencer::client_ui::lobby

#endif
