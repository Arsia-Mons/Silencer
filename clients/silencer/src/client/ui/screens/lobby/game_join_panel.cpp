#include "game_join_panel.h"

#include "client/ui/hooks/use_lobby.h"

#include <utility>

namespace silencer::client_ui::lobby {

void GameJoinPanelInit(GameJoinPanelState & state) {
	state = GameJoinPanelState{};
}

void GameJoinPanelTick(GameJoinPanelState & state,
                       LobbyModel & lobby) {
	if(lobby.pregame.in_lobby()){
		state.readyLabel = lobby.pregame.ready_blocked() ? "Waiting..." : "Ready";
	}else{
		state.readyLabel = "Ready";
	}

	state.rosterRows.clear();
	for(const LobbyPregameRosterRow& modelRow : lobby.pregame.roster()){
		GameJoinRosterRow row;
		row.ready = modelRow.ready;
		row.agency = modelRow.agency;
		row.teamNumber = modelRow.team_number;
		row.peerSlot = modelRow.peer_slot;
		row.drawEmblem = modelRow.draw_emblem;
		row.name = modelRow.name;
		row.level = modelRow.level;
		state.rosterRows.push_back(std::move(row));
	}
}

}  // namespace silencer::client_ui::lobby
