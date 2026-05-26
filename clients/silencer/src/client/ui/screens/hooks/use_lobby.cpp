#include "hooks/use_lobby.h"

#include "client/ui/ClientUi.h"
#include "config.h"
#include "game_state.h"
#include "lobby.h"
#include "password_modal.h"
#include "peer.h"
#include "screen_context.h"
#include "team.h"
#include "ui/runtime/react.h"
#include "user.h"
#include "world.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

namespace silencer::client_ui::hooks {
namespace {

struct LobbyContext {
	ScreenContext * screen = nullptr;
	Lobby * lobby = nullptr;
	World * world = nullptr;
};

struct LobbyAgencyLevel {
	bool found = false;
	uint8_t level = 0;
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

void SendJoinReady(World * world)
{
	if(!world) return;
	world->SendReadyIfAllowed();
}

void ChangeJoinTeam(World * world)
{
	if(!world) return;
	world->ChangeTeam();
}

void BeginTechSelection(World * world)
{
	if(!world) return;
	world->choosingtech = true;
	world->peers.RequestPeerList();
}

void ClearGameJoinActions(LobbyGameJoinActions & actions)
{
	actions.ready = false;
	actions.changeTeam = false;
	actions.chooseTech = false;
}

void FlushGameJoinActions(World * world,
                          const std::shared_ptr<LobbyGameJoinActions> & actions,
                          const std::function<bool()> & gameJoinStillActive,
                          const std::function<void()> & showTech)
{
	if(!actions) return;
	if(gameJoinStillActive && !gameJoinStillActive()){
		ClearGameJoinActions(*actions);
		return;
	}

	if(actions->chooseTech){
		BeginTechSelection(world);
		if(showTech) showTech();
		ClearGameJoinActions(*actions);
		return;
	}
	if(actions->ready){
		SendJoinReady(world);
	}
	if(actions->changeTeam){
		ChangeJoinTeam(world);
	}
	ClearGameJoinActions(*actions);
}

LobbyTechItemDetails TechItemDetailsForIndex(World * world, int itemIndex)
{
	LobbyTechItemDetails details;
	if(!world || itemIndex < 0 || itemIndex >= static_cast<int>(world->buyableitems.size())){
		return details;
	}
	BuyableItem * item = world->buyableitems[itemIndex];
	if(!item) return details;

	details.found = true;
	details.title = "-";
	details.title += item->name;
	details.title += "-";

	char desc[1024];
	std::strncpy(desc, item->description, sizeof(desc));
	desc[sizeof(desc) - 1] = '\0';
	int lineNo = 0;
	char * line = std::strtok(desc, "\n");
	while(line && lineNo < static_cast<int>(details.descriptionLines.size())){
		details.descriptionLines[lineNo++] = line;
		line = std::strtok(nullptr, "\n");
	}
	return details;
}

LobbyCharacterStats CharacterStatsForAgency(Lobby * lobby, uint8_t agency)
{
	LobbyCharacterStats result;
	if(!lobby) return result;

	const Lobby::Character * character = lobby->GetSelectedCharacter();
	result.name = character ? character->name : "No Agent";

	User * user = lobby->GetUserInfo(lobby->accountid);
	if(!user || user->retrieving) return result;
	if(agency >= sizeof(user->agency) / sizeof(user->agency[0])) return result;

	const auto & stats = user->agency[agency];
	result.statsAvailable = true;
	result.maxLevel = stats.level >= User::maxlevel;
	result.wins = stats.wins;
	result.losses = stats.losses;
	result.xpToNextLevel = stats.xptonextlevel;
	result.level = stats.level;
	result.endurance = stats.endurance;
	result.shield = stats.shield;
	result.jetpack = stats.jetpack;
	result.techslots = stats.techslots;
	result.hacking = stats.hacking;
	result.contacts = stats.contacts;
	return result;
}

LobbyAgencyLevel CurrentLobbyAgencyLevel(Lobby * lobby)
{
	LobbyAgencyLevel result;
	if(!lobby) return result;
	User * user = lobby->GetUserInfo(lobby->accountid);
	if(!user) return result;
	const uint8_t agency =
		lobby->GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
	if(agency >= sizeof(user->agency) / sizeof(user->agency[0])) return result;
	result.found = true;
	result.level = user->agency[agency].level;
	return result;
}

void ToggleTechChoice(World * world, int itemIndex)
{
	if(!world) return;
	const Uint8 localId = world->GetLocalPeerId();
	Peer * localPeer = world->GetPeer(localId);
	Team * team = world->GetPeerTeam(localId);
	if(!localPeer || !team || itemIndex < 0
	   || itemIndex >= static_cast<int>(world->buyableitems.size())){
		return;
	}

	BuyableItem * item = world->buyableitems[itemIndex];
	if(!item) return;
	User * user = world->lobby.GetUserInfo(localPeer->accountid);
	if(!user) return;

	const int techSlotsLeft =
		user->agency[team->agency].techslots - world->TechSlotsUsed(*localPeer);
	const bool selected = (localPeer->techchoices & item->techchoice) != 0;
	const bool interactable = (item->techslots <= techSlotsLeft) || selected;
	if(!interactable) return;

	const Uint32 newChoices = localPeer->techchoices ^ item->techchoice;
	world->SetTech(newChoices);
	Config::GetInstance().defaulttechchoices[team->agency] = newChoices;
	Config::GetInstance().Save();
}

void ClearGameTechActions(LobbyGameTechActions & actions)
{
	actions.toggleIndex = -1;
	actions.backToTeams = false;
}

void FlushGameTechActions(World * world,
                          const std::shared_ptr<LobbyGameTechActions> & actions,
                          const std::function<bool()> & gameTechStillActive,
                          const std::function<void()> & showTeams)
{
	if(!actions) return;
	if(gameTechStillActive && !gameTechStillActive()){
		ClearGameTechActions(*actions);
		return;
	}
	if(actions->backToTeams){
		if(showTeams) showTeams();
		ClearGameTechActions(*actions);
		return;
	}
	if(actions->toggleIndex >= 0){
		ToggleTechChoice(world, actions->toggleIndex);
	}
	ClearGameTechActions(*actions);
}

void ClearGameSelectActions(LobbyGameSelectActions & actions)
{
	actions.create = false;
	actions.join = false;
	actions.spectate = false;
}

void JoinLobbyGame(ScreenContext * screen, uint32_t gameId, const char * password)
{
	if(!screen || gameId == 0) return;
	screen->JoinLobbyGameById(gameId, password);
}

void SpectateLobbyGame(ScreenContext * screen, uint32_t gameId, const char * password)
{
	if(!screen || gameId == 0) return;
	screen->SpectateLobbyGameById(gameId, password);
}

void HandleJoinGameSelectAction(ScreenContext * screen, Lobby * lobby, uint32_t gameId)
{
	if(!screen) return;
	ScreenContext::LobbyGameDetails lobbyGame =
		screen->LobbyGameDetailsFor(gameId);
	if(!lobbyGame.found){
		screen->ShowMessage("No game selected");
		return;
	}
	if(!screen->LobbyNetworkIdle()) return;
	LobbyAgencyLevel agencyLevel = CurrentLobbyAgencyLevel(lobby);
	if(agencyLevel.found){
		if(lobbyGame.minLevel > agencyLevel.level){
			screen->ShowMessage("Your player level is too low");
			return;
		}else if(lobbyGame.maxLevel < agencyLevel.level){
			screen->ShowMessage("Your player level is too high");
			return;
		}
	}
	if(lobbyGame.passwordRequiredForLocalAccount){
		screen->PushScreen(std::make_unique<PasswordModal>(
			[screen, gameId](const char * password) {
				JoinLobbyGame(screen, gameId, password ? password : "");
			}));
	}else{
		JoinLobbyGame(screen, lobbyGame.gameId, nullptr);
	}
}

void HandleSpectateGameSelectAction(ScreenContext * screen, uint32_t gameId)
{
	if(!screen) return;
	ScreenContext::LobbyGameDetails lobbyGame =
		screen->LobbyGameDetailsFor(gameId);
	if(!lobbyGame.found){
		screen->ShowMessage("No game selected");
		return;
	}
	if(!screen->LobbyNetworkIdle()) return;
	if(lobbyGame.passwordRequiredForLocalAccount){
		screen->PushScreen(std::make_unique<PasswordModal>(
			[screen, gameId](const char * password) {
				SpectateLobbyGame(screen, gameId, password ? password : "");
			}));
	}else{
		SpectateLobbyGame(screen, lobbyGame.gameId, nullptr);
	}
}

void FlushGameSelectActions(ScreenContext * screen,
                            Lobby * lobby,
                            const std::shared_ptr<LobbyGameSelectActions> & actions,
                            const std::function<bool()> & gameSelectStillActive,
                            const std::function<uint32_t()> & selectedGameId,
                            const std::function<void()> & showCreate)
{
	if(!actions) return;
	if(gameSelectStillActive && !gameSelectStillActive()){
		ClearGameSelectActions(*actions);
		return;
	}

	if(actions->create){
		if(showCreate) showCreate();
		ClearGameSelectActions(*actions);
		return;
	}

	const uint32_t gameId = selectedGameId ? selectedGameId() : 0;
	if(actions->join){
		HandleJoinGameSelectAction(screen, lobby, gameId);
	}
	if(actions->spectate){
		HandleSpectateGameSelectAction(screen, gameId);
	}
	ClearGameSelectActions(*actions);
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
	result.flushGameJoinActions =
		[queueWrite, world](std::shared_ptr<LobbyGameJoinActions> actions,
		                    std::function<bool()> gameJoinStillActive,
		                    std::function<void()> showTech) {
			if(!queueWrite || !actions) return;
			queueWrite([world,
			            actions,
			            gameJoinStillActive = std::move(gameJoinStillActive),
			            showTech = std::move(showTech)]() {
				FlushGameJoinActions(world, actions, gameJoinStillActive, showTech);
			});
		};
	result.techItemDetailsForIndex = [world](int itemIndex) {
		return TechItemDetailsForIndex(world, itemIndex);
	};
	result.characterStatsForAgency = [lobby](uint8_t agency) {
		return CharacterStatsForAgency(lobby, agency);
	};
	result.flushGameTechActions =
		[queueWrite, world](std::shared_ptr<LobbyGameTechActions> actions,
		                    std::function<bool()> gameTechStillActive,
		                    std::function<void()> showTeams) {
			if(!queueWrite || !actions) return;
			queueWrite([world,
			            actions,
			            gameTechStillActive = std::move(gameTechStillActive),
			            showTeams = std::move(showTeams)]() {
				FlushGameTechActions(world, actions, gameTechStillActive, showTeams);
			});
		};
	result.flushGameSelectActions =
		[queueWrite, screen, lobby](std::shared_ptr<LobbyGameSelectActions> actions,
		                            std::function<bool()> gameSelectStillActive,
		                            std::function<uint32_t()> selectedGameId,
		                            std::function<void()> showCreate) {
			if(!queueWrite || !actions) return;
			queueWrite([screen,
			            lobby,
			            actions,
			            gameSelectStillActive = std::move(gameSelectStillActive),
			            selectedGameId = std::move(selectedGameId),
			            showCreate = std::move(showCreate)]() {
				FlushGameSelectActions(
					screen,
					lobby,
					actions,
					gameSelectStillActive,
					selectedGameId,
					showCreate);
			});
		};
	return result;
}

} // namespace silencer::client_ui::hooks
