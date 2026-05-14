#include "game_select_panel.h"

#include "lobby_screen.h"
#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "lobbygame.h"
#include "config.h"
#include "user.h"
#include "password_modal.h"

#include <cstring>
#include <memory>
#include <string>

namespace silencer::client_ui::lobby {

namespace {

constexpr Uint16 kListH     = 265;
constexpr Uint8  kListLineH = 14;

constexpr const char * kActionCreate = "lobby.game_select.create";
constexpr const char * kActionJoin = "lobby.game_select.join";
constexpr const char * kActionSpectate = "lobby.game_select.spectate";
constexpr const char * kActionRowPrefix = "lobby.game_select.row";

bool StartsWith(const std::string & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

void RebuildRows(GameSelectPanelState & state, World & world) {
	Uint32 prevSelectedId = 0;
	if(state.selectedIndex >= 0 && state.selectedIndex < (int)state.rows.size()){
		prevSelectedId = state.rows[state.selectedIndex].gameid;
	}
	state.rows.clear();
	for(LobbyGame * lg : world.lobby.games){
		if(!lg) continue;
		GameSelectPanelState::Row r;
		r.name   = lg->name;
		r.gameid = lg->id;
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

LobbyGame * GetSelectedGame(GameSelectPanelState & state, World & world) {
	if(state.selectedIndex < 0 || state.selectedIndex >= (int)state.rows.size()){
		return nullptr;
	}
	return world.lobby.GetGameById(state.rows[state.selectedIndex].gameid);
}

void RecomputeInfoBlock(GameSelectPanelState & state, World & world) {
	LobbyGame * lobbygame = GetSelectedGame(state, world);
	if(!lobbygame){
		state.infoName.clear();
		state.infoMap.clear();
		state.infoSecurity.clear();
		state.infoCreator.clear();
		state.infoLimits.clear();
		state.joinVisible = false;
		state.spectateVisible = false;
		return;
	}
	state.infoName = lobbygame->name;

	state.infoMap = "Map: ";
	state.infoMap += lobbygame->mapname;

	const char * passwordlock = (strlen(lobbygame->password) > 0)
	                              ? "*PASSWORD LOCK*" : "";
	std::string security = "No";
	switch(lobbygame->securitylevel){
		case LobbyGame::SECLOW:    security = "Low"; break;
		case LobbyGame::SECMEDIUM: security = "Medium"; break;
		case LobbyGame::SECHIGH:   security = "High"; break;
	}
	state.infoSecurity = security + " Security";
	while(state.infoSecurity.length() < 21){
		state.infoSecurity += " ";
	}
	state.infoSecurity += passwordlock;

	User * creator = world.lobby.GetUserInfo(lobbygame->accountid);
	state.infoCreator = "Creator: ";
	if(creator) state.infoCreator += creator->name;

	const bool ingame = lobbygame->state == 1;
	if(!ingame){
		state.infoLimits =
			"MinLv:" + std::to_string(lobbygame->minlevel)
			+ " MaxLv:" + std::to_string(lobbygame->maxlevel)
			+ " MaxPl:" + std::to_string(lobbygame->maxplayers)
			+ " MaxTm:" + std::to_string(lobbygame->maxteams);
	}else{
		state.infoLimits.clear();
	}

	state.joinVisible = false;
	state.spectateVisible = false;
	if(!ingame && lobbygame->players < lobbygame->maxplayers){
		state.joinVisible = true;
	}else if(ingame && lobbygame->canrejoin){
		state.joinVisible = true;
	}
	if(ingame && lobbygame->spectatable){
		state.spectateVisible = true;
	}
}

void HandleJoinClick(GameSelectPanelState & state, World & world, ScreenContext & ctx) {
	LobbyGame * lobbygame = GetSelectedGame(state, world);
	if(!lobbygame){
		ctx.ShowMessage("No game selected");
		return;
	}
	if(!world.IsIdle()) return;
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	bool canjoin = true;
	if(user){
		if(lobbygame->minlevel > user->agency[Config::GetInstance().defaultagency].level){
			canjoin = false;
			ctx.ShowMessage("Your player level is too low");
		}else if(lobbygame->maxlevel < user->agency[Config::GetInstance().defaultagency].level){
			canjoin = false;
			ctx.ShowMessage("Your player level is too high");
		}
	}
	if(!canjoin) return;
	ctx.game.currentlobbygameid = lobbygame->id;
	if(strlen(lobbygame->password) > 0 && lobbygame->accountid != world.lobby.accountid){
		Uint32 gameId = lobbygame->id;
		ctx.PushScreen(std::make_unique<PasswordModal>(
			[&ctx, gameId](const char * password){
				LobbyGame * lg = ctx.world.lobby.GetGameById(gameId);
				if(lg){
					char buf[64];
					std::strncpy(buf, password ? password : "", sizeof(buf) - 1);
					buf[sizeof(buf) - 1] = '\0';
					ctx.game.JoinGame(*lg, buf);
				}
			}));
	}else{
		ctx.game.JoinGame(*lobbygame);
	}
}

void HandleSpectateClick(GameSelectPanelState & state, World & world, ScreenContext & ctx) {
	LobbyGame * lobbygame = GetSelectedGame(state, world);
	if(!lobbygame){
		ctx.ShowMessage("No game selected");
		return;
	}
	if(!world.IsIdle()) return;
	ctx.game.currentlobbygameid = lobbygame->id;
	if(strlen(lobbygame->password) > 0 && lobbygame->accountid != world.lobby.accountid){
		Uint32 gameId = lobbygame->id;
		ctx.PushScreen(std::make_unique<PasswordModal>(
			[&ctx, gameId](const char * password){
				LobbyGame * lg = ctx.world.lobby.GetGameById(gameId);
				if(lg){
					char buf[64];
					std::strncpy(buf, password ? password : "", sizeof(buf) - 1);
					buf[sizeof(buf) - 1] = '\0';
					ctx.game.SpectateGame(*lg, buf);
				}
			}));
	}else{
		ctx.game.SpectateGame(*lobbygame);
	}
}

}  // namespace

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
                         World & world,
                         ScreenContext & ctx,
                         LobbyScreen & owner) {
	if(!world.lobby.gamesprocessed){
		RebuildRows(state, world);
		world.lobby.gamesprocessed = true;
	}

	if(state.rowClickedIndex >= 0){
		state.selectedIndex = state.rowClickedIndex;
		state.rowClickedIndex = -1;
	}

	RecomputeInfoBlock(state, world);

	if(state.createClicked){
		state.createClicked = false;
		owner.ShowGameCreate(ctx);
		return;
	}
	if(state.joinClicked){
		state.joinClicked = false;
		HandleJoinClick(state, world, ctx);
	}
	if(state.spectateClicked){
		state.spectateClicked = false;
		HandleSpectateClick(state, world, ctx);
	}
}

bool GameSelectPanelHandleUiIntent(GameSelectPanelState & state,
                                   const silencer::ui::UiAction & action) {
	if(action.kind == silencer::ui::UiActionKind::Activate){
		if(action.id == kActionCreate){
			state.createClicked = true;
			return true;
		}
		if(action.id == kActionJoin){
			state.joinClicked = true;
			return true;
		}
		if(action.id == kActionSpectate){
			state.spectateClicked = true;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::Select &&
	   StartsWith(action.id, kActionRowPrefix)){
		state.rowClickedIndex = action.index;
		return true;
	}
	return false;
}

}  // namespace silencer::client_ui::lobby
