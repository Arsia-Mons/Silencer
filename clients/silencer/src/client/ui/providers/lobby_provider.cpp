#include "client/ui/hooks/use_lobby.h"
#include "client/ui/hooks/use_navigation.h"

#include "buyableitem.h"
#include "character_create_screen.h"
#include "config.h"
#include "game.h"
#include "lobbygame.h"
#include "map_downloader.h"
#include "mapfetch.h"
#include "message_modal.h"
#include "os.h"
#include "password_modal.h"
#include "peer.h"
#include "screen_context.h"
#include "serializer.h"
#include "team.h"
#include "updater.h"
#include "user.h"
#include "world.h"
#include "ambience_mixer.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <utility>

namespace silencer {
namespace client_ui {

LobbyProviderValue MakeLobbyProvider(ScreenContext& ctx, LobbyScreen * screen) {
	LobbyProviderValue value;
	value.world = &ctx.world;
	value.game = &ctx.game;
	value.ambience = &ctx.ambienceMixer;
	value.map_downloader = &ctx.mapDownloader;
	value.updater = &ctx.updater;
	(void)screen;
	return value;
}

namespace lobby_provider_detail {

World * LobbyWorld(const LobbyProviderValue& provider) {
	return provider.world;
}

Game * LobbyGameOwner(const LobbyProviderValue& provider) {
	return provider.game;
}

AmbienceMixer * LobbyAmbience(const LobbyProviderValue& provider) {
	return provider.ambience;
}

MapDownloader * LobbyMapDownloader(const LobbyProviderValue& provider) {
	return provider.map_downloader;
}

Updater * LobbyUpdater(const LobbyProviderValue& provider) {
	return provider.updater;
}

Navigation LobbyNavigation(const LobbyProviderValue& provider) {
	(void)provider;
	return use_navigation();
}

void CopyPassword(char * dst, int dstLen, const char * password) {
	if(!dst || dstLen <= 0) return;
	std::strncpy(dst, password ? password : "", static_cast<size_t>(dstLen - 1));
	dst[dstLen - 1] = '\0';
}

bool CheckJoinLevel(const LobbyProviderValue& provider,
                    World& world,
                    LobbyGame& game) {
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(!user) return true;
	const Uint8 agency =
		world.lobby.GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
	if(game.minlevel > user->agency[agency].level){
		LobbyModalModel(provider).show_message("Your player level is too low");
		return false;
	}
	if(game.maxlevel < user->agency[agency].level){
		LobbyModalModel(provider).show_message("Your player level is too high");
		return false;
	}
	return true;
}

void AddLine(LobbyConnectionTickResult& result, const char * text) {
	if(text) result.log_lines.push_back(text);
}

void AddMotdLines(LobbyConnectionTickResult& result, const char * motd) {
	if(!motd) return;
	std::string text = motd;
	size_t start = 0;
	while(start < text.size()){
		size_t end = text.find('\n', start);
		std::string line = text.substr(start, end == std::string::npos ? end : end - start);
		if(!line.empty()) result.log_lines.push_back(line);
		if(end == std::string::npos) break;
		start = end + 1;
	}
}

LobbyAgentSummary AgentSummaryFrom(const Lobby::Character& character) {
	LobbyAgentSummary out;
	out.id = character.id;
	out.name = character.name;
	out.agency = character.agencyIdx;
	out.rename_available = character.renameAvailable;
	out.stats.wins = character.stats.wins;
	out.stats.losses = character.stats.losses;
	out.stats.xp = character.stats.xp;
	out.stats.level = character.stats.level;
	out.stats.endurance = character.stats.endurance;
	out.stats.shield = character.stats.shield;
	out.stats.jetpack = character.stats.jetpack;
	out.stats.techslots = character.stats.techslots;
	out.stats.hacking = character.stats.hacking;
	out.stats.contacts = character.stats.contacts;
	return out;
}

struct TechContext {
	World * world = nullptr;
	Uint8 local_id = 0;
	Peer * local_peer = nullptr;
	Team * team = nullptr;
	int slots_left = 0;
	bool has_slots = false;
};

TechContext ResolveTechContext(const LobbyProviderValue& provider) {
	TechContext out;
	out.world = LobbyWorld(provider);
	if(!out.world) return out;
	out.local_id = out.world->peers.localpeerid;
	out.local_peer = out.world->peers.peerlist[out.local_id];
	out.team = out.world->GetPeerTeam(out.local_id);
	if(out.local_peer && out.team){
		User * user = out.world->lobby.GetUserInfo(out.local_peer->accountid);
		if(user){
			out.slots_left = user->agency[out.team->agency].techslots
			               - out.world->TechSlotsUsed(*out.local_peer);
			out.has_slots = true;
		}
	}
	return out;
}

bool TechItemAvailableForTeam(const BuyableItem * item, const Team * team) {
	return item && item->techslots &&
	       team &&
	       (item->agencyspecific == -1 || item->agencyspecific == team->agency);
}

}  // namespace lobby_provider_detail

LobbyConnectionModel::LobbyConnectionModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

void LobbyConnectionModel::reset() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return;
	world->DestroyAllObjects();
	world->lobby.ClearGames();
	world->lobby.state = Lobby::WAITING;
}

bool LobbyConnectionModel::ready() const {
	AmbienceMixer * ambience = lobby_provider_detail::LobbyAmbience(provider_);
	return ambience && ambience->FadedIn();
}

bool LobbyConnectionModel::credentials_pending() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	return world && world->lobby.state == Lobby::AUTHSENT;
}

LobbyConnectionTickResult LobbyConnectionModel::tick(bool motd_printed) const {
	LobbyConnectionTickResult result;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	Updater * updater = lobby_provider_detail::LobbyUpdater(provider_);
	if(!world) return result;

	world->lobby.LockMutex();
	switch(world->lobby.state){
		case Lobby::CONNECTING:
		break;
		case Lobby::WAITINGFORRESOLVER:
		break;
		case Lobby::AUTHSENT:
		break;
		case Lobby::IDLE:
		break;
		case Lobby::WAITING:{
			char line[128];
			snprintf(line, sizeof(line), "Connecting to %s:%d",
			         Config::GetInstance().lobbyhost,
			         Config::GetInstance().lobbyport);
			lobby_provider_detail::AddLine(result, line);
			world->lobby.Connect(Config::GetInstance().lobbyhost,
			                     Config::GetInstance().lobbyport);
		}break;
		case Lobby::RESOLVING:
			lobby_provider_detail::AddLine(result, "Resolving hostname...");
			world->lobby.state = Lobby::WAITINGFORRESOLVER;
		break;
		case Lobby::RESOLVEFAILED:
			lobby_provider_detail::AddLine(result, "Could not resolve hostname");
			world->lobby.state = Lobby::IDLE;
		break;
		case Lobby::RESOLVED:
			lobby_provider_detail::AddLine(result, "Hostname resolved");
			world->lobby.Connect(Config::GetInstance().lobbyhost,
			                     Config::GetInstance().lobbyport);
		break;
		case Lobby::CONNECTED:
			lobby_provider_detail::AddLine(result, "Connected");
			lobby_provider_detail::AddLine(result, "Checking version...");
			world->lobby.SendVersion();
			world->lobby.state = Lobby::CHECKINGVERSION;
		break;
		case Lobby::CHECKINGVERSION:
			if(world->lobby.versionchecked){
				if(world->lobby.versionok){
					lobby_provider_detail::AddLine(result, "Software version is current");
					world->lobby.state = Lobby::AUTHENTICATING;
				}else{
					if(world->lobby.updateavailable && updater){
						updater->PresentUpdate(world->lobby.updateurl,
						                       world->lobby.updatesha256);
						world->lobby.Disconnect();
						world->lobby.state = Lobby::IDLE;
						result.destination = LobbyConnectionDestination::Update;
					}else{
						lobby_provider_detail::AddLine(result, "Software is out of date");
						lobby_provider_detail::AddLine(result, "Get latest version at:");
						lobby_provider_detail::AddLine(result, "https://github.com/Arsia-Mons/Silencer");
						world->lobby.Disconnect();
						world->lobby.state = Lobby::IDLE;
					}
				}
			}
		break;
		case Lobby::AUTHENTICATING:
		break;
		case Lobby::AUTHFAILED:
			lobby_provider_detail::AddLine(result, "Authentication failed");
			if(strlen(world->lobby.failmessage) > 0){
				lobby_provider_detail::AddLine(result, world->lobby.failmessage);
			}
			world->lobby.state = Lobby::AUTHENTICATING;
		break;
		case Lobby::AUTHENTICATED:
			if(!world->lobby.charactersreceived){
				break;
			}
			lobby_provider_detail::AddLine(result, "Authenticated");
			result.destination = world->lobby.characters.empty()
				? LobbyConnectionDestination::CharacterCreate
				: LobbyConnectionDestination::Lobby;
		break;
		case Lobby::CONNECTIONFAILED:
			lobby_provider_detail::AddLine(result, "Connection failed");
			world->lobby.state = Lobby::IDLE;
		break;
		case Lobby::DISCONNECTED:
			lobby_provider_detail::AddLine(result, "Disconnected");
			world->lobby.state = Lobby::IDLE;
		break;
	}
	if(world->lobby.motdreceived && !motd_printed){
		lobby_provider_detail::AddMotdLines(result, world->lobby.motd);
		result.motd_printed = true;
	}
	world->lobby.UnlockMutex();
	return result;
}

void LobbyConnectionModel::submit_credentials(
		const char * username,
		const char * password) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return;
	world->lobby.LockMutex();
	if(world->lobby.state == Lobby::AUTHENTICATING){
		world->lobby.SetLocalUsername(username);
		world->lobby.SendCredentials(username, password);
		world->lobby.state = Lobby::AUTHSENT;
	}
	world->lobby.UnlockMutex();
}

void LobbyConnectionModel::cancel() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(world) world->lobby.Disconnect();
}

LobbyAgentsModel::LobbyAgentsModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

std::vector<LobbyAgentSummary> LobbyAgentsModel::list() const {
	std::vector<LobbyAgentSummary> agents;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return agents;
	world->lobby.LockMutex();
	agents.reserve(world->lobby.characters.size());
	for(const Lobby::Character& character : world->lobby.characters){
		agents.push_back(lobby_provider_detail::AgentSummaryFrom(character));
	}
	world->lobby.UnlockMutex();
	return agents;
}

size_t LobbyAgentsModel::count() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return 0;
	world->lobby.LockMutex();
	const size_t n = world->lobby.characters.size();
	world->lobby.UnlockMutex();
	return n;
}

bool LobbyAgentsModel::has_any() const {
	return count() > 0;
}

bool LobbyAgentsModel::select(int index) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return false;
	Uint32 agentId = 0;
	world->lobby.LockMutex();
	if(index >= 0 && index < static_cast<int>(world->lobby.characters.size())){
		const Lobby::Character& character =
			world->lobby.characters[static_cast<size_t>(index)];
		agentId = character.id;
		world->lobby.selectedcharid = character.id;
		world->lobby.selectedagency = character.agencyIdx;
	}
	if(agentId != 0){
		world->lobby.SelectCharacter(agentId);
	}
	world->lobby.UnlockMutex();
	return agentId != 0;
}

bool LobbyAgentsModel::rename_start(int index, LobbyAgentSummary& agent) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return false;
	bool found = false;
	world->lobby.LockMutex();
	if(index >= 0 && index < static_cast<int>(world->lobby.characters.size())){
		const Lobby::Character& character =
			world->lobby.characters[static_cast<size_t>(index)];
		if(character.renameAvailable){
			agent = lobby_provider_detail::AgentSummaryFrom(character);
			found = true;
		}
	}
	world->lobby.UnlockMutex();
	return found;
}

LobbyAgentCreateStatus LobbyAgentsModel::create_status(size_t count_on_entry) const {
	LobbyAgentCreateStatus status;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return status;
	world->lobby.LockMutex();
	status.received = world->lobby.charactersreceived;
	status.count = world->lobby.characters.size();
	status.created = status.count > count_on_entry;
	world->lobby.UnlockMutex();
	return status;
}

LobbyAgentRenameStatus LobbyAgentsModel::rename_status(Uint32 agent_id) const {
	LobbyAgentRenameStatus status;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return status;
	world->lobby.LockMutex();
	status.received = world->lobby.charactersreceived;
	for(size_t i = 0; i < world->lobby.characters.size(); ++i){
		const Lobby::Character& character = world->lobby.characters[i];
		if(character.id == agent_id){
			status.renamed = !character.renameAvailable;
			status.renamed_index = static_cast<int>(i);
			break;
		}
	}
	world->lobby.UnlockMutex();
	return status;
}

void LobbyAgentsModel::create(const char * alias, Uint8 agency) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return;
	world->lobby.LockMutex();
	world->lobby.charactersreceived = false;
	world->lobby.CreateCharacter(alias, agency);
	world->lobby.UnlockMutex();
}

void LobbyAgentsModel::rename(Uint32 agent_id, const char * alias) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return;
	world->lobby.LockMutex();
	world->lobby.charactersreceived = false;
	world->lobby.RenameCharacter(agent_id, alias);
	world->lobby.UnlockMutex();
}

LobbyCharacterModel::LobbyCharacterModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

Uint8 LobbyCharacterModel::default_agency() const {
	return Config::GetInstance().defaultagency;
}

Uint8 LobbyCharacterModel::agency_for_index(int index) const {
	static const Uint8 agencies[5] = {
		Team::NOXIS,
		Team::LAZARUS,
		Team::CALIBER,
		Team::STATIC,
		Team::BLACKROSE,
	};
	if(index < 0 || index >= 5) return agencies[0];
	return agencies[index];
}

Uint8 LobbyCharacterModel::selected_agency() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return default_agency();
	return world->lobby.GetSelectedAgencyOrDefault(default_agency());
}

bool LobbyCharacterModel::agent_selection_locked() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	return world && world->IsConnected();
}

void LobbyCharacterModel::apply_selected_agency(Uint8 agency) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(world && world->IsConnected()){
		world->SetAgency(agency);
	}
}

LobbyCharacterPanelSnapshot LobbyCharacterModel::panel(Uint8 agency) const {
	LobbyCharacterPanelSnapshot snapshot;
	snapshot.agency = agency;
	snapshot.agent_selection_locked = agent_selection_locked();

	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return snapshot;

	const Lobby::Character * character = world->lobby.GetSelectedCharacter();
	if(character){
		snapshot.agent_name = character->name;
	}

	User * user = world->lobby.GetUserInfo(world->lobby.accountid);
	if(user && !user->retrieving){
		const auto& stats = user->agency[agency];
		snapshot.progress.wins = stats.wins;
		snapshot.progress.losses = stats.losses;
		snapshot.progress.xptonextlevel = stats.xptonextlevel;
		snapshot.progress.level = stats.level;
		snapshot.progress.endurance = stats.endurance;
		snapshot.progress.shield = stats.shield;
		snapshot.progress.jetpack = stats.jetpack;
		snapshot.progress.techslots = stats.techslots;
		snapshot.progress.hacking = stats.hacking;
		snapshot.progress.contacts = stats.contacts;
		snapshot.progress.max_level = stats.level >= User::maxlevel;
		snapshot.progress.loaded = true;
	}
	return snapshot;
}

LobbyChatModel::LobbyChatModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

LobbyChatPump LobbyChatModel::pump() const {
	LobbyChatPump out;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return out;

	if(world->lobby.channelchanged){
		if(world->lobby.lastchannel[0] == '\0'){
			std::strcpy(world->lobby.lastchannel, world->lobby.channel);
		}
		out.channel_changed = true;
		out.channel = world->lobby.channel;
		world->lobby.channelchanged = false;
	}

	if(world->lobby.presencechanged || !world->lobby.gamesprocessed){
		out.presence_changed = true;
		out.presence.reserve(world->lobby.presence.size());
		for(auto & kv : world->lobby.presence){
			Lobby::PresenceEntry & entry = kv.second;
			LobbyChatPresenceRow row;
			row.label = entry.name;
			row.group = (entry.status <= 2) ? entry.status : 0;
			if(entry.gameid != 0){
				LobbyGame * game = world->lobby.GetGameById(entry.gameid);
				if(game){
					row.label += " [";
					row.label += game->name;
					row.label += "]";
				}
			}
			out.presence.push_back(std::move(row));
		}
		std::sort(out.presence.begin(), out.presence.end(),
			[](const LobbyChatPresenceRow& a, const LobbyChatPresenceRow& b){
				if(a.group != b.group) return a.group < b.group;
				return a.label < b.label;
			});
		world->lobby.presencechanged = false;
	}

	while(!world->lobby.chatmessages.empty()){
		auto message = world->lobby.chatmessages.front();
		const char * msgtext = message.data();
		size_t msglen = std::strlen(msgtext);
		LobbyChatMessage chat;
		chat.text = msgtext ? std::string(msgtext) : std::string();
		chat.color = static_cast<Uint8>(message[msglen + 1]);
		chat.brightness = static_cast<Uint8>(message[msglen + 2]);
		out.messages.push_back(std::move(chat));
		world->lobby.chatmessages.pop_front();
	}
	return out;
}

void LobbyChatModel::send(const char * message) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world || !message || message[0] == '\0') return;
	world->lobby.SendChat(world->lobby.channel, message);
}

LobbySessionModel::LobbySessionModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

bool LobbySessionModel::disconnect_lobby_if_needed() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world || world->lobby.state != Lobby::DISCONNECTED) return false;
	world->Disconnect();
	return true;
}

LobbySessionPumpResult LobbySessionModel::pump(
		bool in_join_or_tech_panel,
		bool has_progress_modal) const {
	LobbySessionPumpResult result;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	Game * game = lobby_provider_detail::LobbyGameOwner(provider_);
	MapDownloader * maps = lobby_provider_detail::LobbyMapDownloader(provider_);
	AmbienceMixer * ambience = lobby_provider_detail::LobbyAmbience(provider_);
	if(!world || !game || !maps) return result;

	if(world->lobby.state == Lobby::DISCONNECTED){
		world->Disconnect();
		result.lobby_disconnected = true;
		return result;
	}

	if(!in_join_or_tech_panel){
		if(game->joininggame){
			if(world->IsConnected()){
				game->joininggame = false;
			}
			if(world->IsIdle()){
				game->joininggame = false;
				result.dismiss_progress = true;
				result.message = "Unable to join game";
			}
		}

		if(has_progress_modal){
			result.progress_text =
				(maps->mapUploadState.load(std::memory_order_relaxed) == 1)
					? "Uploading map" : "Creating game";
			int dots = (world->tickcount / 4) % 6;
			if(dots > 3) dots = 6 - dots;
			for(int i = 0; i < dots; i++) result.progress_text += ".";
		}

		if(has_progress_modal &&
		   world->lobby.creategamestatus != 100 &&
		   maps->mapUploadState.load(std::memory_order_relaxed) == 0 &&
		   (world->IsConnected() || world->IsIdle())){
			result.dismiss_progress = true;
			game->creategameclicked = false;
		}

		if(world->IsConnected()){
			Peer * peer = world->peers.peerlist[world->peers.localpeerid];
			if(peer){
				maps->mapexistchecked = false;
				maps->mapjoingeneration.fetch_add(1, std::memory_order_relaxed);
				maps->mapjoinstate.store(0, std::memory_order_relaxed);
				if(maps->mapjointhread.joinable()) maps->mapjointhread.detach();

				const Uint8 agency = world->lobby.GetSelectedAgencyOrDefault(
					Config::GetInstance().defaultagency);
				world->SetTech(Config::GetInstance().defaulttechchoices[agency]);

				LobbyGame * lobbygame = world->lobby.GetGameById(game->currentlobbygameid);
				if(lobbygame && ambience){
					char channel[256];
					ambience->GetGameChannelName(*lobbygame, channel);
					std::strcpy(world->lobby.lastchannel, world->lobby.channel);
					world->lobby.JoinChannel(channel);
					result.map_name = lobbygame->mapname;
				}
				result.show_game_join = true;
			}
		}
	}

	maps->ProcessMapDownload();

	if(in_join_or_tech_panel && !world->IsConnected()){
		result.disconnected_from_game = true;
	}
	return result;
}

LobbyModalModel::LobbyModalModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

void LobbyModalModel::show_message(const char * message) const {
	lobby_provider_detail::LobbyNavigation(provider_).push(
		std::make_unique<MessageModal>(message ? message : ""));
}

LobbyBrowserModel::LobbyBrowserModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

void LobbyBrowserModel::mark_games_dirty() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(world){
		world->lobby.gamesprocessed = false;
	}
}

LobbyBrowserRowsSnapshot LobbyBrowserModel::refresh_rows() const {
	LobbyBrowserRowsSnapshot out;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world || world->lobby.gamesprocessed) return out;

	out.rebuilt = true;
	for(LobbyGame * game : world->lobby.games){
		if(!game) continue;
		LobbyBrowserGameRow row;
		row.id = game->id;
		row.name = game->name;
		out.rows.push_back(std::move(row));
	}
	world->lobby.gamesprocessed = true;
	return out;
}

LobbyBrowserGameInfo LobbyBrowserModel::info(Uint32 game_id) const {
	LobbyBrowserGameInfo out;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world || game_id == 0) return out;
	LobbyGame * game = world->lobby.GetGameById(game_id);
	if(!game) return out;

	out.name = game->name;
	out.map = "Map: ";
	out.map += game->mapname;

	const char * passwordLock = (std::strlen(game->password) > 0)
		? "*PASSWORD LOCK*" : "";
	std::string security = "No";
	switch(game->securitylevel){
		case LobbyGame::SECLOW:    security = "Low"; break;
		case LobbyGame::SECMEDIUM: security = "Medium"; break;
		case LobbyGame::SECHIGH:   security = "High"; break;
	}
	out.security = security + " Security";
	while(out.security.length() < 21){
		out.security += " ";
	}
	out.security += passwordLock;

	User * creator = world->lobby.GetUserInfo(game->accountid);
	out.creator = "Creator: ";
	if(creator) out.creator += creator->name;

	const bool ingame = game->state == 1;
	if(!ingame){
		out.limits =
			"MinLv:" + std::to_string(game->minlevel)
			+ " MaxLv:" + std::to_string(game->maxlevel)
			+ " MaxPl:" + std::to_string(game->maxplayers)
			+ " MaxTm:" + std::to_string(game->maxteams);
	}

	if(!ingame && game->players < game->maxplayers){
		out.join_visible = true;
	}else if(ingame && game->canrejoin){
		out.join_visible = true;
	}
	if(ingame && game->spectatable){
		out.spectate_visible = true;
	}
	return out;
}

void LobbyBrowserModel::join(Uint32 game_id) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	Game * owner = lobby_provider_detail::LobbyGameOwner(provider_);
	if(!world || !owner) return;
	LobbyGame * game = world->lobby.GetGameById(game_id);
	if(!game){
		LobbyModalModel(provider_).show_message("No game selected");
		return;
	}
	if(!world->IsIdle()) return;
	if(!lobby_provider_detail::CheckJoinLevel(provider_, *world, *game)){
		return;
	}
	owner->currentlobbygameid = game->id;
	if(std::strlen(game->password) > 0 && game->accountid != world->lobby.accountid){
		Uint32 gameId = game->id;
		lobby_provider_detail::LobbyNavigation(provider_).push(
			std::make_unique<PasswordModal>(
				[world, owner, gameId](const char * password){
					LobbyGame * selected = world->lobby.GetGameById(gameId);
					if(!selected) return;
					char buf[64];
					lobby_provider_detail::CopyPassword(buf, static_cast<int>(sizeof(buf)), password);
					owner->JoinGame(*selected, buf);
				}));
	}else{
		owner->JoinGame(*game);
	}
}

void LobbyBrowserModel::spectate(Uint32 game_id) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	Game * owner = lobby_provider_detail::LobbyGameOwner(provider_);
	if(!world || !owner) return;
	LobbyGame * game = world->lobby.GetGameById(game_id);
	if(!game){
		LobbyModalModel(provider_).show_message("No game selected");
		return;
	}
	if(!world->IsIdle()) return;
	owner->currentlobbygameid = game->id;
	if(std::strlen(game->password) > 0 && game->accountid != world->lobby.accountid){
		Uint32 gameId = game->id;
		lobby_provider_detail::LobbyNavigation(provider_).push(
			std::make_unique<PasswordModal>(
				[world, owner, gameId](const char * password){
					LobbyGame * selected = world->lobby.GetGameById(gameId);
					if(!selected) return;
					char buf[64];
					lobby_provider_detail::CopyPassword(buf, static_cast<int>(sizeof(buf)), password);
					owner->SpectateGame(*selected, buf);
				}));
	}else{
		owner->SpectateGame(*game);
	}
}

LobbyCreateModel::LobbyCreateModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

LobbyCreateModel::Defaults LobbyCreateModel::defaults() const {
	Defaults out;
	out.spectatable = Config::GetInstance().lastspectatable;
	out.game_name = Config::GetInstance().defaultgamename;
	MapDownloader * maps = lobby_provider_detail::LobbyMapDownloader(provider_);
	if(!maps) return out;

	std::vector<std::string> localMaps;
	CDResDir();
	auto files = maps->ListFiles((GetResDir() + "level").c_str());
	localMaps.insert(localMaps.end(), files.begin(), files.end());
	CDDataDir();
	files = maps->ListFiles((GetDataDir() + "level/download").c_str());
	for(auto & file : files){
		if(std::find(localMaps.begin(), localMaps.end(), file) == localMaps.end()){
			localMaps.push_back(file);
		}
	}
	std::sort(localMaps.begin(), localMaps.end());
	out.maps = localMaps;

	maps->servermaps.clear();
	for(auto & entry : FetchServerMapList(Config::GetInstance().mapapiurl)){
		if(std::find(localMaps.begin(), localMaps.end(), entry.first) == localMaps.end()){
			std::string label = "[DL] " + entry.first;
			out.maps.push_back(label);
			maps->servermaps[label] = entry.second;
		}
	}
	return out;
}

void LobbyCreateModel::reset() const {
	MapDownloader * maps = lobby_provider_detail::LobbyMapDownloader(provider_);
	if(maps) maps->selectedmap = -1;
	if(Game * game = lobby_provider_detail::LobbyGameOwner(provider_)){
		game->creategameclicked = false;
	}
}

void LobbyCreateModel::select_map(int index) const {
	if(MapDownloader * maps = lobby_provider_detail::LobbyMapDownloader(provider_)){
		maps->selectedmap = index;
	}
}

void LobbyCreateModel::set_spectatable(bool spectatable) const {
	Config::GetInstance().lastspectatable = spectatable;
	Config::GetInstance().Save();
}

std::string LobbyCreateModel::preview_map_path(const std::string& map_label) const {
	MapDownloader * maps = lobby_provider_detail::LobbyMapDownloader(provider_);
	if(!maps || maps->servermaps.count(map_label) > 0) return std::string();
	return maps->FindMap(map_label.c_str());
}

LobbyCreateModel::PumpResult LobbyCreateModel::pump() const {
	PumpResult result;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	MapDownloader * maps = lobby_provider_detail::LobbyMapDownloader(provider_);
	Game * game = lobby_provider_detail::LobbyGameOwner(provider_);
	if(!world || !maps || !game) return result;

	int uploadState = maps->mapUploadState.load(std::memory_order_acquire);
	if(uploadState == 2){
		maps->mapUploadState.store(0, std::memory_order_relaxed);
		const char * password = maps->pendingCreate.password.empty()
			? nullptr : maps->pendingCreate.password.c_str();
		world->lobby.CreateGame(
			maps->pendingCreate.gamename.c_str(),
			maps->pendingCreate.mapname.c_str(),
			maps->pendingCreate.maphash,
			password,
			maps->pendingCreate.securitylevel,
			maps->pendingCreate.minlevel,
			maps->pendingCreate.maxlevel,
			maps->pendingCreate.maxplayers,
			maps->pendingCreate.maxteams,
			maps->pendingCreate.spectatable);
	}else if(uploadState == 3){
		maps->mapUploadState.store(0, std::memory_order_relaxed);
		game->creategameclicked = false;
		result.dismiss_progress = true;
		result.message = "Could not upload map";
		return result;
	}

	if(world->lobby.creategamestatus == 1 && game->creategameclicked){
		world->lobby.creategamestatus = 0;
		game->creategameclicked = false;
		LobbyGame * lobbygame = world->lobby.GetGameById(world->lobby.createdgameid);
		if(lobbygame){
			Serializer data;
			lobbygame->Serialize(Serializer::WRITE, data);
			world->gameinfo.Serialize(Serializer::READ, data);
			game->JoinGame(*lobbygame, lobbygame->password);
			maps->LoadMapData(maps->FindMap(lobbygame->mapname, &lobbygame->maphash).c_str());
			game->currentlobbygameid = lobbygame->id;
		}
	}else if(world->lobby.creategamestatus != 100 &&
	         world->lobby.creategamestatus != 0 &&
	         game->creategameclicked){
		world->lobby.creategamestatus = 0;
		game->creategameclicked = false;
		result.dismiss_progress = true;
		result.message = "Could not create game";
	}
	return result;
}

LobbyCreateModel::StartResult LobbyCreateModel::start(const Request& request) const {
	StartResult result;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	MapDownloader * maps = lobby_provider_detail::LobbyMapDownloader(provider_);
	Game * game = lobby_provider_detail::LobbyGameOwner(provider_);
	if(!world || !maps || !game) return result;
	if(game->creategameclicked) return result;
	if(request.game_name.empty()){
		result.message = "No game name";
		return result;
	}
	if(request.map_name.empty()){
		result.message = "No map selected";
		return result;
	}
	if(maps->servermaps.count(request.map_name) > 0){
		result.message = "Download the map first";
		return result;
	}

	Uint8 securitylevel = LobbyGame::SECNONE;
	switch(request.security_index){
		case 1: securitylevel = LobbyGame::SECLOW;    break;
		case 2: securitylevel = LobbyGame::SECMEDIUM; break;
		case 3: securitylevel = LobbyGame::SECHIGH;   break;
	}

	unsigned char maphash[20];
	maps->CalculateMapHash(maps->FindMap(request.map_name.c_str()).c_str(), &maphash);
	auto & pending = maps->pendingCreate;
	pending.gamename = request.game_name;
	pending.mapname = request.map_name;
	pending.password = request.password;
	std::memcpy(pending.maphash, maphash, 20);
	pending.securitylevel = securitylevel;
	pending.minlevel = request.min_level;
	pending.maxlevel = request.max_level;
	pending.maxplayers = request.max_players;
	pending.maxteams = request.max_teams;
	pending.spectatable = request.spectatable;

	if(maps->mapUploadThread.joinable()) maps->mapUploadThread.detach();
	uint32_t generation = ++maps->mapUploadGeneration;
	std::string mapPath = maps->FindMap(request.map_name.c_str());
	std::string dataDir = GetDataDir();
	bool isBundledMap = dataDir.empty() || mapPath.substr(0, dataDir.size()) != dataDir;
	if(isBundledMap){
		maps->mapUploadState.store(2, std::memory_order_release);
	}else{
		maps->mapUploadState.store(1, std::memory_order_relaxed);
		std::string apiURL = Config::GetInstance().mapapiurl;
		std::atomic<int> * uploadState = &maps->mapUploadState;
		std::atomic<uint32_t> * uploadGeneration = &maps->mapUploadGeneration;
		std::string mapName = request.map_name;
		maps->mapUploadThread = std::thread([mapName, mapPath, apiURL, generation, uploadState, uploadGeneration](){
			bool ok = UploadMapToServer(mapName.c_str(), mapPath.c_str(), apiURL.c_str());
			if(uploadGeneration->load(std::memory_order_relaxed) != generation) return;
			uploadState->store(ok ? 2 : 3, std::memory_order_release);
		});
	}
	world->lobby.creategamestatus = 0;
	game->creategameclicked = true;
	std::strncpy(Config::GetInstance().defaultgamename,
	             request.game_name.c_str(),
	             sizeof(Config::GetInstance().defaultgamename) - 1);
	Config::GetInstance().defaultgamename[sizeof(Config::GetInstance().defaultgamename) - 1] = '\0';
	Config::GetInstance().Save();
	result.started = true;
	return result;
}

LobbyPregameTeamModel::LobbyPregameTeamModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

void LobbyPregameTeamModel::change() const {
	if(World * world = lobby_provider_detail::LobbyWorld(provider_)){
		world->ChangeTeam();
	}
}

LobbyPregameTechModel::LobbyPregameTechModel(const LobbyProviderValue& provider)
	: provider_(provider) {}

Uint8 LobbyPregameTechModel::local_peer_id() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	return world ? world->peers.localpeerid : 0;
}

Peer * LobbyPregameTechModel::peer(Uint8 peer_id) const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	return world ? world->peers.peerlist[peer_id] : nullptr;
}

void LobbyPregameTechModel::request_peer_list() const {
	if(World * world = lobby_provider_detail::LobbyWorld(provider_)){
		world->choosingtech = true;
		world->RequestPeerList();
	}
}

void LobbyPregameTechModel::set_choices(Uint32 choices) const {
	if(World * world = lobby_provider_detail::LobbyWorld(provider_)){
		world->SetTech(choices);
	}
}

LobbyPregameTechModel::Status LobbyPregameTechModel::status() const {
	Status out;
	lobby_provider_detail::TechContext tech =
		lobby_provider_detail::ResolveTechContext(provider_);
	if(!tech.world) return out;
	if(tech.has_slots){
		out.slots_left = "Tech slots left: " + std::to_string(tech.slots_left);
	}else if(!tech.local_peer && tech.world->tickcount % 12 == 0){
		request_peer_list();
	}

	if(tech.team){
		int peerIndex = 0;
		for(int i = 0; i < 4 && peerIndex < 3; i++){
			if(tech.team->peers[i] == tech.local_id) continue;
			if(i >= tech.team->numpeers){
				peerIndex++;
				continue;
			}
			Peer * peer = tech.world->peers.peerlist[tech.team->peers[i]];
			User * user = peer ? tech.world->lobby.GetUserInfo(peer->accountid) : nullptr;
			out.peer_names[peerIndex] = user ? std::string(user->DisplayName()) : std::string();
			peerIndex++;
		}
	}
	return out;
}

LobbyPregameTechModel::Description
LobbyPregameTechModel::description(int item_index) const {
	Description out;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world || item_index < 0 ||
	   item_index >= static_cast<int>(world->buyableitems.size())){
		return out;
	}
	BuyableItem * item = world->buyableitems[static_cast<size_t>(item_index)];
	if(!item) return out;
	out.name = "-";
	out.name += item->name;
	out.name += "-";

	char desc[1024];
	std::strncpy(desc, item->description, sizeof(desc));
	desc[sizeof(desc) - 1] = '\0';
	int lineNo = 0;
	char * line = std::strtok(desc, "\n");
	while(line && lineNo < static_cast<int>(out.lines.size())){
		out.lines[static_cast<size_t>(lineNo++)] = line;
		line = std::strtok(nullptr, "\n");
	}
	return out;
}

void LobbyPregameTechModel::toggle(int item_index) const {
	lobby_provider_detail::TechContext tech =
		lobby_provider_detail::ResolveTechContext(provider_);
	if(!tech.world || !tech.local_peer || !tech.team || item_index < 0 ||
	   item_index >= static_cast<int>(tech.world->buyableitems.size())){
		return;
	}
	BuyableItem * item = tech.world->buyableitems[static_cast<size_t>(item_index)];
	if(!item) return;
	const bool interactable = (item->techslots <= tech.slots_left)
	                       || ((tech.local_peer->techchoices & item->techchoice) != 0);
	if(!interactable) return;

	const Uint32 newChoices = tech.local_peer->techchoices ^ item->techchoice;
	set_choices(newChoices);
	Config::GetInstance().defaulttechchoices[tech.team->agency] = newChoices;
	Config::GetInstance().Save();
}

LobbyPregameTechModel::Grid LobbyPregameTechModel::grid() const {
	Grid out;
	lobby_provider_detail::TechContext tech =
		lobby_provider_detail::ResolveTechContext(provider_);
	if(!tech.world || !tech.team) return out;
	out.visible = true;

	struct ColAssign { int peer_slot; bool draw; bool local; };
	ColAssign cols[4] = { {-1,false,false}, {-1,false,false},
	                       {-1,false,false}, {-1,false,false} };
	int peerIndex = 0;
	for(int i = 0; i < 4; i++){
		const bool local = (tech.team->peers[i] == tech.local_id);
		const bool draw = (i < tech.team->numpeers);
		const int col = local ? 3 : peerIndex;
		if(!local) peerIndex++;
		if(col >= 0 && col < 4){
			cols[col].peer_slot = i;
			cols[col].draw = draw;
			cols[col].local = local;
		}
	}
	out.local_labels_visible = cols[3].draw;

	for(size_t itemIndex = 0; itemIndex < tech.world->buyableitems.size(); ++itemIndex){
		BuyableItem * item = tech.world->buyableitems[itemIndex];
		if(!lobby_provider_detail::TechItemAvailableForTeam(item, tech.team)){
			continue;
		}

		GridRow row;
		row.item_index = static_cast<int>(itemIndex);
		row.label = item->name;
		row.label += " (";
		row.label += std::to_string(item->techslots);
		row.label += ")";

		for(int col = 0; col < 4; ++col){
			GridCell cell;
			cell.draw = cols[col].draw;
			cell.local = cols[col].local;
			if(cell.draw){
				Peer * peer = tech.world->peers.peerlist[tech.team->peers[cols[col].peer_slot]];
				cell.selected = peer && (peer->techchoices & item->techchoice);
				const bool interactable =
					cell.local &&
					((item->techslots <= tech.slots_left) ||
					 (peer && (peer->techchoices & item->techchoice)));
				cell.brightness = interactable ? 128 : 64;
				if(cell.local){
					row.label_brightness = cell.brightness;
				}
			}
			row.cells[static_cast<size_t>(col)] = cell;
		}
		out.rows.push_back(std::move(row));
	}
	return out;
}

LobbyPregameModel::LobbyPregameModel(const LobbyProviderValue& provider)
	: team(provider), tech(provider), provider_(provider) {}

bool LobbyPregameModel::in_lobby() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	return world && world->gameplaystate == World::INLOBBY;
}

bool LobbyPregameModel::ready_blocked() const {
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return false;
	Peer * localpeer = world->peers.peerlist[world->peers.localpeerid];
	return localpeer && localpeer->ishost && !world->AllPeersDownloadedMap();
}

std::vector<LobbyPregameRosterRow> LobbyPregameModel::roster() const {
	std::vector<LobbyPregameRosterRow> rows;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world || !world->IsConnected()) return rows;

	const std::vector<Uint16>& teamIds = world->GetObjectsByType(ObjectTypes::TEAM);
	for(Uint16 teamId : teamIds){
		Team * team = static_cast<Team *>(world->GetObjectFromId(teamId));
		if(!team || team->numpeers == 0) continue;
		bool drewEmblem = false;
		for(int i = 0; i < team->numpeers; ++i){
			Peer * peer = world->GetPeer(team->peers[i]);
			if(!peer || peer->observer || peer->disconnected) continue;
			User * user = world->lobby.GetUserInfo(peer->accountid);
			if(!user || user->retrieving || !user->DisplayName()[0]) continue;

			LobbyPregameRosterRow row;
			row.ready = peer->isready;
			row.agency = team->agency;
			row.team_number = team->number;
			row.peer_slot = static_cast<Uint8>(i);
			row.draw_emblem = !drewEmblem;
			row.name = peer->isbot ? std::string(user->DisplayName()) + " [BOT]"
			                       : std::string(user->DisplayName());
			row.level = "L:" + std::to_string(user->agency[team->agency].level);
			rows.push_back(std::move(row));
			drewEmblem = true;
		}
	}
	return rows;
}

void LobbyPregameModel::set_ready(bool ready) const {
	if(!ready) return;
	World * world = lobby_provider_detail::LobbyWorld(provider_);
	if(!world) return;
	Peer * localpeer = world->peers.peerlist[world->peers.localpeerid];
	const bool ishost = localpeer && localpeer->ishost;
	if(!ishost || world->AllPeersDownloadedMap()){
		world->SendReady();
	}
}

void LobbyPregameModel::leave_joined_game() const {
	if(Game * game = lobby_provider_detail::LobbyGameOwner(provider_)){
		game->LeaveJoinedGame();
	}
}

LobbyModel::LobbyModel(const LobbyProviderValue& provider)
	: connection(provider), agents(provider), character(provider), chat(provider),
	  session(provider), modal(provider), browser(provider), create(provider),
	  pregame(provider) {}

LobbyModel use_lobby(const LobbyProviderValue& provider) {
	return LobbyModel(provider);
}

}  // namespace client_ui
}  // namespace silencer
