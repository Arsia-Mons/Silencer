#include "hooks/use_lobby.h"

#include "client/ui/ClientUi.h"
#include "game_state.h"
#include "lobby.h"
#include "screen_context.h"
#include "ui/runtime/react.h"
#include "user.h"
#include "world.h"

#include <cstdint>
#include <utility>

namespace silencer::client_ui::hooks {
namespace {

struct LobbyContext {
	ScreenContext * screen = nullptr;
	Lobby * lobby = nullptr;
	World * world = nullptr;
};

ReactContext g_lobbyContextValue = {};

constexpr Lobby::StatID kMissionSummaryUpgradeStatIds[6] = {
	Lobby::STAT_ENDURANCE,
	Lobby::STAT_SHIELD,
	Lobby::STAT_JETPACK,
	Lobby::STAT_TECHSLOTS,
	Lobby::STAT_HACKING,
	Lobby::STAT_CONTACTS,
};

void SubmitCredentials(Lobby * lobby, const std::string & username, const std::string & password)
{
	if(!lobby) return;
	lobby->LockMutex();
	if(lobby->state == Lobby::AUTHENTICATING){
		lobby->SetLocalUsername(username.c_str());
		lobby->SendCredentials(username.c_str(), password.c_str());
		lobby->state = Lobby::AUTHSENT;
	}
	lobby->UnlockMutex();
}

void UpgradeMissionSummaryStat(Lobby * lobby, World * world, int upgradeIndex)
{
	if(!lobby || !world || upgradeIndex < 0 || upgradeIndex >= 6) return;
	User * user = lobby->GetUserInfo(lobby->accountid);
	if(user){
		lobby->UpgradeStat(user->selectedcharid,
		                   user->statsagency,
		                   kMissionSummaryUpgradeStatIds[upgradeIndex]);
	}
}

void CompleteMissionSummary(ScreenContext * screen, Lobby * lobby)
{
	if(!screen || !lobby) return;
	if(lobby->state == Lobby::AUTHENTICATED){
		screen->GoToState(GameState::LOBBY);
		lobby->JoinChannel(lobby->lastchannel);
	}else{
		screen->GoToState(GameState::MAINMENU);
	}
}

} // namespace

void LobbyProvider(ScreenContext & ctx, const std::function<void()> & children)
{
	LobbyContext lobbyContext{
		&ctx,
		&ctx.lobby,
		&ctx.world,
	};
	REACT_PROVIDER_ENTER_KEY("LobbyProvider", reinterpret_cast<uintptr_t>(&ctx.lobby));
	PROVIDE(&g_lobbyContextValue, &lobbyContext) {
		if(children) children();
	}
	REACT_PROVIDER_EXIT();
}

LobbyUi UseLobby()
{
	auto * context = static_cast<LobbyContext *>(use_context(&g_lobbyContextValue));
	QueueUiWrite queueWrite = UseUiWriteQueue();
	ScreenContext * screen = context ? context->screen : nullptr;
	Lobby * lobby = context ? context->lobby : nullptr;
	World * world = context ? context->world : nullptr;

	LobbyUi result;
	result.authSent = lobby && lobby->state == Lobby::AUTHSENT;
	result.submitCredentials =
		[queueWrite, lobby](std::string username, std::string password) {
			if(!queueWrite || !lobby) return;
			queueWrite([lobby, username = std::move(username), password = std::move(password)]() {
				SubmitCredentials(lobby, username, password);
			});
		};
	result.flushMissionSummaryActions =
		[queueWrite, screen, lobby, world](std::shared_ptr<LobbyMissionSummaryActions> actions) {
			if(!queueWrite || !actions) return;
			queueWrite([screen, lobby, world, actions]() {
				if(actions->upgradeIndex >= 0){
					UpgradeMissionSummaryStat(lobby, world, actions->upgradeIndex);
				}
				if(actions->done){
					CompleteMissionSummary(screen, lobby);
				}
			});
		};
	return result;
}

} // namespace silencer::client_ui::hooks
