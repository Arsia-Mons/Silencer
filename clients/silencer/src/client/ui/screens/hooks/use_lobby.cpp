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

LobbyTechSnapshot GameTechSnapshot(World * world, Lobby * lobby)
{
	LobbyTechSnapshot snapshot;
	if(!world || !lobby) return snapshot;

	const uint8_t localId = world->GetLocalPeerId();
	Peer * localPeer = world->GetPeer(localId);
	Team * team = world->GetPeerTeam(localId);

	int localTechSlotsLeft = 0;
	if(localPeer && team){
		User * user = lobby->GetUserInfo(localPeer->accountid);
		if(user){
			localTechSlotsLeft =
				user->agency[team->agency].techslots - world->TechSlotsUsed(*localPeer);
			snapshot.slotsLeft =
				"Tech slots left: " + std::to_string(localTechSlotsLeft);
		}
	}

	if(!team) return snapshot;

	int peerIndex = 0;
	for(int i = 0; i < 4 && peerIndex < 3; i++){
		if(team->peers[i] == localId) continue;
		if(i >= team->numpeers){ peerIndex++; continue; }
		Peer * peer = world->GetPeer(team->peers[i]);
		User * user = peer ? lobby->GetUserInfo(peer->accountid) : nullptr;
		snapshot.peerNames[peerIndex] =
			user ? std::string(user->DisplayName()) : std::string();
		peerIndex++;
	}

	struct ColAssign { int peerSlot; bool draw; bool isLocal; };
	ColAssign cols[4] = { {-1,false,false}, {-1,false,false},
	                       {-1,false,false}, {-1,false,false} };
	peerIndex = 0;
	for(int i = 0; i < 4; i++){
		const bool isLocal = (team->peers[i] == localId);
		const bool draw = (i < team->numpeers);
		const int col = isLocal ? 3 : peerIndex;
		if(!isLocal) peerIndex++;
		if(col >= 0 && col < 4){
			cols[col].peerSlot = i;
			cols[col].draw = draw;
			cols[col].isLocal = isLocal;
		}
	}

	const ColAssign & localColumn = cols[3];
	for(size_t itemIndex = 0; itemIndex < world->buyableitems.size(); itemIndex++){
		BuyableItem * item = world->buyableitems[itemIndex];
		if(!item || !item->techslots) continue;
		if(item->agencyspecific != -1 && item->agencyspecific != team->agency){
			continue;
		}

		LobbyTechGridRow row;
		row.itemIndex = static_cast<int>(itemIndex);
		if(localColumn.draw){
			row.label = item->name;
			row.label += " (";
			row.label += std::to_string(item->techslots);
			row.label += ")";
		}

		for(int col = 0; col < 4; col++){
			if(!cols[col].draw) continue;
			Peer * peer = world->GetPeer(team->peers[cols[col].peerSlot]);
			const bool selected = peer && (peer->techchoices & item->techchoice);
			bool interactable = false;
			if(cols[col].isLocal){
				interactable =
					(item->techslots <= localTechSlotsLeft) || selected;
			}
			const uint8_t brightness = cols[col].isLocal && interactable ? 128 : 64;
			row.cells[col].draw = true;
			row.cells[col].selected = selected;
			row.cells[col].brightness = brightness;
			if(cols[col].isLocal) row.labelBrightness = brightness;
		}

		snapshot.rows.push_back(std::move(row));
	}

	return snapshot;
}

LobbyCharacterStats CharacterStatsForAgency(Lobby * lobby, uint8_t agency)
{
	LobbyCharacterStats result;
	if(!lobby) return result;

	const Lobby::Character * character = lobby->GetSelectedCharacter();
	result.name = character ? character->name : "No Agent";

	const User * user = lobby->FindUserInfo(lobby->accountid);
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
	const User * user = lobby->FindUserInfo(lobby->accountid);
	if(!user) return result;
	const uint8_t agency =
		lobby->GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
	if(agency >= sizeof(user->agency) / sizeof(user->agency[0])) return result;
	result.found = true;
	result.level = user->agency[agency].level;
	return result;
}

uint8_t SelectedAgencyOrDefault(Lobby * lobby)
{
	const uint8_t fallback = Config::GetInstance().defaultagency;
	return lobby ? lobby->GetSelectedAgencyOrDefault(fallback) : fallback;
}

bool AgentSelectionLocked(World * world)
{
	return world && world->IsConnected();
}

LobbyContext * CurrentLobbyContext()
{
	return static_cast<LobbyContext *>(use_context(&g_lobbyContextValue));
}

void SyncSelectedAgency(World * world, uint8_t agency)
{
	if(!AgentSelectionLocked(world)) return;
	world->SetAgency(agency);
}

void OpenCharacterSelection(ScreenContext * screen, World * world)
{
	if(!screen || AgentSelectionLocked(world)) return;
	screen->GoToState(GameState::CREATECHARACTER);
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

std::string UseLobbyGameJoinReadyLabel()
{
	LobbyContext * context = CurrentLobbyContext();
	World * world = context ? context->world : nullptr;
	return world && world->IsLocalHostWaitingForMapDownloads()
		? "Waiting..."
		: "Ready";
}

std::vector<LobbyGameJoinRosterRow> UseLobbyGameJoinRosterRows()
{
	LobbyContext * context = CurrentLobbyContext();
	World * world = context ? context->world : nullptr;
	Lobby * lobby = context ? context->lobby : nullptr;
	std::vector<LobbyGameJoinRosterRow> rows;
	if(!world || !lobby || !world->IsConnected()) return rows;

	const std::vector<Uint16> & teamIds = world->GetObjectsByType(ObjectTypes::TEAM);
	for(Uint16 teamId : teamIds){
		Team * team = static_cast<Team *>(world->GetObjectFromId(teamId));
		if(!team || team->numpeers == 0) continue;
		bool drewEmblem = false;
		for(int i = 0; i < team->numpeers; ++i){
			Peer * peer = world->GetPeer(team->peers[i]);
			if(!peer || peer->observer || peer->disconnected) continue;
			User * user = lobby->GetUserInfo(peer->accountid);
			if(!user || user->retrieving || !user->DisplayName()[0]) continue;

			LobbyGameJoinRosterRow row;
			row.ready = peer->isready;
			row.agency = team->agency;
			row.teamNumber = team->number;
			row.peerSlot = static_cast<uint8_t>(i);
			row.drawEmblem = !drewEmblem;
			row.name = peer->isbot ? std::string(user->DisplayName()) + " [BOT]"
			                       : std::string(user->DisplayName());
			row.level = "L:" + std::to_string(user->agency[team->agency].level);
			rows.push_back(std::move(row));
			drewEmblem = true;
		}
	}
	return rows;
}

LobbyTechSnapshot UseLobbyGameTechSnapshot()
{
	LobbyContext * context = CurrentLobbyContext();
	return GameTechSnapshot(context ? context->world : nullptr,
	                        context ? context->lobby : nullptr);
}

LobbyTechItemDetails UseLobbyTechItemDetails(int itemIndex)
{
	LobbyContext * context = CurrentLobbyContext();
	return TechItemDetailsForIndex(context ? context->world : nullptr, itemIndex);
}

void ReconcileLobbyCharacterAgency(ScreenContext & ctx, int & lastSyncedAgency)
{
	if(!AgentSelectionLocked(&ctx.world)){
		lastSyncedAgency = -1;
		return;
	}

	const uint8_t selectedAgency = SelectedAgencyOrDefault(&ctx.lobby);
	if(static_cast<int>(selectedAgency) == lastSyncedAgency) return;
	lastSyncedAgency = selectedAgency;
	SyncSelectedAgency(&ctx.world, selectedAgency);
}

void FlushLobbyCharacterSelectionRequest(ScreenContext & ctx, bool & requested)
{
	if(!requested) return;
	requested = false;
	OpenCharacterSelection(&ctx, &ctx.world);
}

void RequestLobbyGameTechPeerList(ScreenContext & ctx)
{
	if(ctx.world.GetPeer(ctx.world.GetLocalPeerId())) return;
	if(ctx.world.tickcount % 12 != 0) return;
	ctx.world.peers.RequestPeerList();
}

LobbyUi UseLobby()
{
	auto * context = CurrentLobbyContext();
	QueueUiWrite queueWrite = UseUiWriteQueue();
	ScreenContext * screen = context ? context->screen : nullptr;
	Lobby * lobby = context ? context->lobby : nullptr;
	World * world = context ? context->world : nullptr;

	LobbyUi result;
	result.authSent = lobby && lobby->state == Lobby::AUTHSENT;
	result.selectedAgency = SelectedAgencyOrDefault(lobby);
	result.agentSelectionLocked = AgentSelectionLocked(world);
	result.submitCredentials =
		[queueWrite, lobby](std::string username, std::string password) {
			if(!queueWrite || !lobby) return;
			queueWrite([lobby, username = std::move(username), password = std::move(password)]() {
				SubmitCredentials(lobby, username, password);
			});
		};
	result.upgradeMissionSummaryStat =
		[queueWrite, lobby, world](int upgradeIndex) {
			if(!queueWrite) return;
			queueWrite([lobby, world, upgradeIndex]() {
				UpgradeMissionSummaryStat(lobby, world, upgradeIndex);
			});
		};
	result.completeMissionSummary =
		[queueWrite, screen, lobby]() {
			if(!queueWrite) return;
			queueWrite([screen, lobby]() {
				CompleteMissionSummary(screen, lobby);
			});
		};
	result.sendGameJoinReady =
		[queueWrite, world]() {
			if(!queueWrite) return;
			queueWrite([world]() {
				SendJoinReady(world);
			});
		};
	result.changeGameJoinTeam =
		[queueWrite, world]() {
			if(!queueWrite) return;
			queueWrite([world]() {
				ChangeJoinTeam(world);
			});
		};
	result.beginGameTechSelection =
		[queueWrite, world]() {
			if(!queueWrite) return;
			queueWrite([world]() {
				BeginTechSelection(world);
			});
		};
	result.characterStatsForAgency = [lobby](uint8_t agency) {
		return CharacterStatsForAgency(lobby, agency);
	};
	result.toggleGameTechChoice =
		[queueWrite, world](int itemIndex) {
			if(!queueWrite) return;
			queueWrite([world, itemIndex]() {
				ToggleTechChoice(world, itemIndex);
			});
		};
	result.joinLobbyGame =
		[queueWrite, screen, lobby](uint32_t gameId) {
			if(!queueWrite) return;
			queueWrite([screen, lobby, gameId]() {
				HandleJoinGameSelectAction(screen, lobby, gameId);
			});
		};
	result.spectateLobbyGame =
		[queueWrite, screen](uint32_t gameId) {
			if(!queueWrite) return;
			queueWrite([screen, gameId]() {
				HandleSpectateGameSelectAction(screen, gameId);
			});
		};
	return result;
}

} // namespace silencer::client_ui::hooks
