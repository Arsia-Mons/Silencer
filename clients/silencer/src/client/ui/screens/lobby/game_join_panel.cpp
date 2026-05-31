#include "game_join_panel.h"

#include "lobby_screen.h"
#include "objecttypes.h"
#include "screen_context.h"
#include "team.h"
#include "user.h"
#include "world.h"

namespace silencer::client_ui::lobby {

namespace game_join_panel_detail {

constexpr const char * kActionTech = "lobby.game_join.choose_tech";
constexpr const char * kActionTeam = "lobby.game_join.change_team";
constexpr const char * kActionReady = "lobby.game_join.ready";

}  // namespace game_join_panel_detail

void GameJoinPanelInit(GameJoinPanelState & state) {
	state = GameJoinPanelState{};
}

void GameJoinPanelTick(GameJoinPanelState & state,
                       World & world,
                       ScreenContext & ctx,
                       LobbyScreen & owner) {
	if(owner.JoinPanelInLobby(world)){
		state.readyLabel = owner.JoinPanelReadyBlocked(world) ? "Waiting..." : "Ready";
	}else{
		state.readyLabel = "Ready";
	}

	state.rosterRows.clear();
	if(world.IsConnected()){
		const std::vector<Uint16> & teamIds = world.GetObjectsByType(ObjectTypes::TEAM);
		for(Uint16 teamId : teamIds){
			Team * team = static_cast<Team *>(world.GetObjectFromId(teamId));
			if(!team || team->numpeers == 0) continue;
			bool drewEmblem = false;
			for(int i = 0; i < team->numpeers; ++i){
				Peer * peer = world.GetPeer(team->peers[i]);
				if(!peer || peer->observer || peer->disconnected) continue;
				User * user = world.lobby.GetUserInfo(peer->accountid);
				if(!user || user->retrieving || !user->DisplayName()[0]) continue;

				GameJoinRosterRow row;
				row.ready = peer->isready;
				row.agency = team->agency;
				row.teamNumber = team->number;
				row.peerSlot = static_cast<Uint8>(i);
				row.drawEmblem = !drewEmblem;
				row.name = peer->isbot ? std::string(user->DisplayName()) + " [BOT]"
				                       : std::string(user->DisplayName());
				row.level = "L:" + std::to_string(user->agency[team->agency].level);
				state.rosterRows.push_back(row);
				drewEmblem = true;
			}
		}
	}

	if(state.techClicked){
		state.techClicked = false;
		owner.ShowGameTech(ctx);
		return;
	}
	if(state.readyClicked){
		state.readyClicked = false;
		owner.JoinPanelSendReady(world);
	}
	if(state.teamClicked){
		state.teamClicked = false;
		owner.JoinPanelChangeTeam(world);
	}
}

bool GameJoinPanelHandleUiIntent(GameJoinPanelState & state,
                                 const silencer::ui::UiAction & action) {
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == game_join_panel_detail::kActionTech){
		GameJoinPanelRequestTech(state);
		return true;
	}
	if(action.id == game_join_panel_detail::kActionTeam){
		GameJoinPanelRequestTeam(state);
		return true;
	}
	if(action.id == game_join_panel_detail::kActionReady){
		GameJoinPanelRequestReady(state);
		return true;
	}
	return false;
}

void GameJoinPanelRequestTech(GameJoinPanelState & state) {
	state.techClicked = true;
}

void GameJoinPanelRequestTeam(GameJoinPanelState & state) {
	state.teamClicked = true;
}

void GameJoinPanelRequestReady(GameJoinPanelState & state) {
	state.readyClicked = true;
}

}  // namespace silencer::client_ui::lobby
