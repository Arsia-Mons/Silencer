#include "game_select_panel.h"

#include "screen_context.h"
#include "password_modal.h"

#include <cstring>
#include <memory>
#include <string>

namespace silencer::client_ui::lobby {

namespace game_select_panel_detail {

constexpr Uint16 kListH     = 265;
constexpr Uint8  kListLineH = 14;

constexpr const char * kActionCreate = "lobby.game_select.create";
constexpr const char * kActionJoin = "lobby.game_select.join";
constexpr const char * kActionSpectate = "lobby.game_select.spectate";
constexpr const char * kActionRowPrefix = "lobby.game_select.row";

template <typename Text>
bool StartsWith(const Text & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

void RebuildRows(GameSelectPanelState & state, ScreenContext & ctx) {
	Uint32 prevSelectedId = 0;
	if(state.selectedIndex >= 0 && state.selectedIndex < (int)state.rows.size()){
		prevSelectedId = state.rows[state.selectedIndex].gameid;
	}
	state.rows.clear();
	for(const ScreenContext::LobbyGameListRow & lobbyGame : ctx.LobbyGameListRows()){
		GameSelectPanelState::Row r;
		r.name   = lobbyGame.name;
		r.gameid = lobbyGame.gameId;
		state.rows.push_back(std::move(r));
	}
	state.selectedIndex = -1;
	if(prevSelectedId != 0){
		for(size_t i = 0; i < state.rows.size(); ++i){
			if(state.rows[i].gameid == prevSelectedId){
				state.selectedIndex = static_cast<int>(i);
				break;
			}
		}
	}
	const int visible = kListH / kListLineH;
	int maxScroll = static_cast<int>(state.rows.size()) - visible;
	if(maxScroll < 0) maxScroll = 0;
	if(state.scrollPos > maxScroll) state.scrollPos = static_cast<Uint16>(maxScroll);
}

Uint32 GetSelectedGameId(const GameSelectPanelState & state) {
	if(state.selectedIndex < 0 || state.selectedIndex >= (int)state.rows.size()){
		return 0;
	}
	return state.rows[state.selectedIndex].gameid;
}

void RecomputeInfoBlock(GameSelectPanelState & state, ScreenContext & ctx) {
	ScreenContext::LobbyGameDetails lobbyGame =
		ctx.LobbyGameDetailsFor(GetSelectedGameId(state));
	if(!lobbyGame.found){
		state.infoName.clear();
		state.infoMap.clear();
		state.infoSecurity.clear();
		state.infoCreator.clear();
		state.infoLimits.clear();
		state.joinVisible = false;
		state.spectateVisible = false;
		return;
	}
	state.infoName = lobbyGame.name;

	state.infoMap = "Map: ";
	state.infoMap += lobbyGame.mapName;

	const char * passwordlock = lobbyGame.passwordProtected
	                              ? "*PASSWORD LOCK*" : "";
	std::string security = "No";
	switch(lobbyGame.securityLevel){
		case ScreenContext::LobbyGameSecurityLevel::Low:
			security = "Low";
			break;
		case ScreenContext::LobbyGameSecurityLevel::Medium:
			security = "Medium";
			break;
		case ScreenContext::LobbyGameSecurityLevel::High:
			security = "High";
			break;
		case ScreenContext::LobbyGameSecurityLevel::None:
			break;
	}
	state.infoSecurity = security + " Security";
	while(state.infoSecurity.length() < 21){
		state.infoSecurity += " ";
	}
	state.infoSecurity += passwordlock;

	state.infoCreator = "Creator: ";
	state.infoCreator += lobbyGame.creatorName;

	if(!lobbyGame.inGame){
		state.infoLimits =
			"MinLv:" + std::to_string(lobbyGame.minLevel)
			+ " MaxLv:" + std::to_string(lobbyGame.maxLevel)
			+ " MaxPl:" + std::to_string(lobbyGame.maxPlayers)
			+ " MaxTm:" + std::to_string(lobbyGame.maxTeams);
	}else{
		state.infoLimits.clear();
	}

	state.joinVisible = false;
	state.spectateVisible = false;
	if(!lobbyGame.inGame && lobbyGame.players < lobbyGame.maxPlayers){
		state.joinVisible = true;
	}else if(lobbyGame.inGame && lobbyGame.canRejoin){
		state.joinVisible = true;
	}
	if(lobbyGame.inGame && lobbyGame.spectatable){
		state.spectateVisible = true;
	}
}

void HandleJoinClick(GameSelectPanelState & state, ScreenContext & ctx) {
	ScreenContext::LobbyGameDetails lobbyGame =
		ctx.LobbyGameDetailsFor(GetSelectedGameId(state));
	if(!lobbyGame.found){
		ctx.ShowMessage("No game selected");
		return;
	}
	if(!ctx.LobbyNetworkIdle()) return;
	ScreenContext::LocalLobbyAgencyLevel agencyLevel = ctx.CurrentLobbyAgencyLevel();
	if(agencyLevel.found){
		if(lobbyGame.minLevel > agencyLevel.level){
			ctx.ShowMessage("Your player level is too low");
			return;
		}else if(lobbyGame.maxLevel < agencyLevel.level){
			ctx.ShowMessage("Your player level is too high");
			return;
		}
	}
	if(lobbyGame.passwordRequiredForLocalAccount){
		Uint32 gameId = lobbyGame.gameId;
		ctx.PushScreen(std::make_unique<PasswordModal>(
			[&ctx, gameId](const char * password){
				ctx.JoinLobbyGameById(gameId, password ? password : "");
			}));
	}else{
		ctx.JoinLobbyGameById(lobbyGame.gameId);
	}
}

void HandleSpectateClick(GameSelectPanelState & state, ScreenContext & ctx) {
	ScreenContext::LobbyGameDetails lobbyGame =
		ctx.LobbyGameDetailsFor(GetSelectedGameId(state));
	if(!lobbyGame.found){
		ctx.ShowMessage("No game selected");
		return;
	}
	if(!ctx.LobbyNetworkIdle()) return;
	if(lobbyGame.passwordRequiredForLocalAccount){
		Uint32 gameId = lobbyGame.gameId;
		ctx.PushScreen(std::make_unique<PasswordModal>(
			[&ctx, gameId](const char * password){
				ctx.SpectateLobbyGameById(gameId, password ? password : "");
			}));
	}else{
		ctx.SpectateLobbyGameById(lobbyGame.gameId);
	}
}

}  // namespace game_select_panel_detail

void GameSelectPanelInit(GameSelectPanelState & state) {
	state.rows.clear();
	state.selectedIndex   = -1;
	state.scrollPos       = 0;
	state.joinClicked     = false;
	state.spectateClicked = false;
	state.createClicked   = false;
	state.rowClickedIndex = -1;
	state.infoName.clear();
	state.infoMap.clear();
	state.infoSecurity.clear();
	state.infoCreator.clear();
	state.infoLimits.clear();
	state.joinVisible     = false;
	state.spectateVisible = false;
}

void GameSelectPanelTick(GameSelectPanelState & state,
                         ScreenContext & ctx) {
	if(ctx.ConsumeLobbyGameListRefresh()){
		game_select_panel_detail::RebuildRows(state, ctx);
	}

	if(state.rowClickedIndex >= 0){
		state.selectedIndex = state.rowClickedIndex;
		state.rowClickedIndex = -1;
	}

	game_select_panel_detail::RecomputeInfoBlock(state, ctx);

	if(state.joinClicked){
		state.joinClicked = false;
		game_select_panel_detail::HandleJoinClick(state, ctx);
	}
	if(state.spectateClicked){
		state.spectateClicked = false;
		game_select_panel_detail::HandleSpectateClick(state, ctx);
	}
}

bool GameSelectPanelHandleUiIntent(GameSelectPanelState & state,
                                   const silencer::ui::UiAction & action) {
	if(action.kind == silencer::ui::UiActionKind::Activate){
		if(action.id == game_select_panel_detail::kActionCreate){
			state.createClicked = true;
			return true;
		}
		if(action.id == game_select_panel_detail::kActionJoin){
			state.joinClicked = true;
			return true;
		}
		if(action.id == game_select_panel_detail::kActionSpectate){
			state.spectateClicked = true;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::Select &&
	   game_select_panel_detail::StartsWith(action.id, game_select_panel_detail::kActionRowPrefix)){
		state.rowClickedIndex = action.index;
		return true;
	}
	return false;
}

}  // namespace silencer::client_ui::lobby
