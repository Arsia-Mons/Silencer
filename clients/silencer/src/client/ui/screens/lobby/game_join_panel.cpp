#include "game_join_panel.h"

#include "client/ui/hooks/use_lobby.h"

#include <utility>

namespace silencer::client_ui::lobby {

namespace game_join_panel_detail {

constexpr const char * kActionTech = "lobby.game_join.choose_tech";
constexpr const char * kActionTeam = "lobby.game_join.change_team";
constexpr const char * kActionReady = "lobby.game_join.ready";

}  // namespace game_join_panel_detail

void GameJoinPanelInit(GameJoinPanelState & state) {
	state = GameJoinPanelState{};
}

GameJoinPanelTickResult GameJoinPanelTick(GameJoinPanelState & state,
                                          LobbyModel & lobby) {
	GameJoinPanelTickResult result;
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

	if(state.techClicked){
		state.techClicked = false;
		lobby.pregame.tech.request_peer_list();
		result.show_tech = true;
		return result;
	}
	if(state.readyClicked){
		state.readyClicked = false;
		lobby.pregame.set_ready(true);
	}
	if(state.teamClicked){
		state.teamClicked = false;
		lobby.pregame.team.change();
	}
	return result;
}

bool GameJoinPanelHandleUiIntent(GameJoinPanelState & state,
                                 const silencer::ui::UiAction & action) {
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == game_join_panel_detail::kActionTech){
		state.techClicked = true;
		return true;
	}
	if(action.id == game_join_panel_detail::kActionTeam){
		state.teamClicked = true;
		return true;
	}
	if(action.id == game_join_panel_detail::kActionReady){
		state.readyClicked = true;
		return true;
	}
	return false;
}

}  // namespace silencer::client_ui::lobby
