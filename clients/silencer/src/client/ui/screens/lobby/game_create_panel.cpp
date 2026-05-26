#include "game_create_panel.h"

#include "lobby_screen.h"
#include "screen_context.h"
#include "world.h"
#include "lobby.h"
#include "lobbygame.h"
#include "screen.h"
#include "config.h"
#include "os.h"
#include "map_downloader.h"
#include "mapfetch.h"
#include "message_modal.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace silencer::client_ui::lobby {

namespace game_create_panel_detail {

constexpr const char * kActionSecurity = "lobby.game_create.security";
constexpr const char * kActionSpectatable = "lobby.game_create.spectatable";
constexpr const char * kActionCreate = "lobby.game_create.create";
constexpr const char * kActionMapPrefix = "lobby.game_create.map";
constexpr const char * kActionMinLevel = "lobby.game_create.min_level";
constexpr const char * kActionMaxLevel = "lobby.game_create.max_level";
constexpr const char * kActionMaxPlayers = "lobby.game_create.max_players";
constexpr const char * kActionMaxTeams = "lobby.game_create.max_teams";
constexpr const char * kActionName = "lobby.game_create.name";
constexpr const char * kActionPassword = "lobby.game_create.password";
constexpr const char * kActionOptionsScroll = kGameCreateOptionsScrollId;

template <typename Text>
bool StartsWith(const Text & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

template <typename Text>
void CopyUiText(char * dst, int dstLen, const Text & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
}

void BuildMapList(GameCreatePanelState & state, ScreenContext & ctx) {
	state.maps.clear();
	MapDownloader & mapDownloader = ctx.mapDownloader;
	std::vector<std::string> maps;
	CDResDir();
	auto files = mapDownloader.ListFiles((GetResDir() + "level").c_str());
	maps.insert(maps.end(), files.begin(), files.end());
	CDDataDir();
	files = mapDownloader.ListFiles((GetDataDir() + "level/download").c_str());
	for(auto & f : files){
		if(std::find(maps.begin(), maps.end(), f) == maps.end()) maps.push_back(f);
	}
	std::sort(maps.begin(), maps.end());
	for(auto & m : maps) state.maps.push_back(m);
	mapDownloader.servermaps.clear();
	for(auto & entry : FetchServerMapList(Config::GetInstance().mapapiurl)){
		if(std::find(maps.begin(), maps.end(), entry.first) == maps.end()){
			std::string label = "[DL] " + entry.first;
			state.maps.push_back(label);
			mapDownloader.servermaps[label] = entry.second;
		}
	}
}

}  // namespace game_create_panel_detail

void GameCreatePanelInit(GameCreatePanelState & state, ScreenContext & ctx) {
	state = GameCreatePanelState{};
	state.spectatable = Config::GetInstance().lastspectatable;
	std::strncpy(state.name, Config::GetInstance().defaultgamename, sizeof(state.name) - 1);
	state.name[sizeof(state.name) - 1] = '\0';
	game_create_panel_detail::BuildMapList(state, ctx);
	ctx.mapDownloader.selectedmap = -1;
	ctx.SetCreateGamePending(false);
}

void GameCreatePanelTick(GameCreatePanelState & state,
                         World & world,
                         ScreenContext & ctx,
                         LobbyScreen & owner) {
	MapDownloader & mapDownloader = ctx.mapDownloader;

	if(state.mapRowClickedIndex >= 0){
		state.mapSelectedIndex = state.mapRowClickedIndex;
		mapDownloader.selectedmap = state.mapRowClickedIndex;
		state.mapRowClickedIndex = -1;
	}
	if(state.securityClicked){
		state.securityClicked = false;
		state.securityIndex = static_cast<Uint8>((state.securityIndex + 1) % 4);
	}
	if(state.spectatableClicked){
		state.spectatableClicked = false;
		state.spectatable = !state.spectatable;
		Config::GetInstance().lastspectatable = state.spectatable;
		Config::GetInstance().Save();
	}

	int us = mapDownloader.mapUploadState.load(std::memory_order_acquire);
	if(us == 2){
		mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
		const char * pw = mapDownloader.pendingCreate.password.empty() ? nullptr : mapDownloader.pendingCreate.password.c_str();
		world.lobby.CreateGame(
			mapDownloader.pendingCreate.gamename.c_str(),
			mapDownloader.pendingCreate.mapname.c_str(),
			mapDownloader.pendingCreate.maphash,
			pw,
			mapDownloader.pendingCreate.securitylevel,
			mapDownloader.pendingCreate.minlevel,
			mapDownloader.pendingCreate.maxlevel,
			mapDownloader.pendingCreate.maxplayers,
			mapDownloader.pendingCreate.maxteams,
			mapDownloader.pendingCreate.spectatable);
	}else if(us == 3){
		mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
		ctx.SetCreateGamePending(false);
		Screen * top = ctx.TopScreen();
		MessageModal * m = dynamic_cast<MessageModal *>(top);
		if(m && m->IsProgress()) ctx.PopScreen();
		ctx.ShowMessage("Could not upload map");
	}
	if(world.lobby.creategamestatus == 1 && ctx.IsCreateGamePending()){
		world.lobby.creategamestatus = 0;
		ctx.SetCreateGamePending(false);
		LobbyGame * lobbygame = world.lobby.GetGameById(world.lobby.createdgameid);
		if(lobbygame){
			owner.SeedHostGameInfo(world, *lobbygame);
			ctx.JoinGame(*lobbygame, lobbygame->password);
			mapDownloader.LoadMapData(mapDownloader.FindMap(lobbygame->mapname, &lobbygame->maphash).c_str());
			ctx.SetCurrentLobbyGameId(lobbygame->id);
		}
	}else if(world.lobby.creategamestatus != 100 && world.lobby.creategamestatus != 0 && ctx.IsCreateGamePending()){
		world.lobby.creategamestatus = 0;
		ctx.SetCreateGamePending(false);
		Screen * top = ctx.TopScreen();
		MessageModal * m = dynamic_cast<MessageModal *>(top);
		if(m && m->IsProgress()) ctx.PopScreen();
		ctx.ShowMessage("Could not create game");
	}

	if(!state.createClicked) return;
	state.createClicked = false;
	if(ctx.IsCreateGamePending()) return;

	if(strlen(state.name) == 0){ ctx.ShowMessage("No game name"); return; }
	if(state.mapSelectedIndex < 0 || state.mapSelectedIndex >= (int)state.maps.size()){
		ctx.ShowMessage("No map selected"); return;
	}
	std::string mapname = state.maps[state.mapSelectedIndex];
	if(mapDownloader.servermaps.count(mapname) > 0){
		ctx.ShowMessage("Download the map first"); return;
	}

	Uint8 securitylevel = LobbyGame::SECNONE;
	switch(state.securityIndex){
		case 1: securitylevel = LobbyGame::SECLOW;    break;
		case 2: securitylevel = LobbyGame::SECMEDIUM; break;
		case 3: securitylevel = LobbyGame::SECHIGH;   break;
	}
	Uint8 maxplayers = static_cast<Uint8>(atoi(state.maxPlayers)); if(maxplayers <= 0) maxplayers = 1;
	Uint8 maxteams   = static_cast<Uint8>(atoi(state.maxTeams));   if(maxteams   <= 0) maxteams   = 1;

	unsigned char maphash[20];
	mapDownloader.CalculateMapHash(mapDownloader.FindMap(mapname.c_str()).c_str(), &maphash);
	auto & pc = mapDownloader.pendingCreate;
	pc.gamename      = state.name;
	pc.mapname       = mapname;
	pc.password      = state.password;
	memcpy(pc.maphash, maphash, 20);
	pc.securitylevel = securitylevel;
	pc.minlevel      = static_cast<Uint8>(atoi(state.minLevel));
	pc.maxlevel      = static_cast<Uint8>(atoi(state.maxLevel));
	pc.maxplayers    = maxplayers;
	pc.maxteams      = maxteams;
	pc.spectatable   = state.spectatable;

	if(mapDownloader.mapUploadThread.joinable()) mapDownloader.mapUploadThread.detach();
	uint32_t gen = ++mapDownloader.mapUploadGeneration;
	std::string mppath = mapDownloader.FindMap(mapname.c_str());
	std::string dataDir = GetDataDir();
	bool isBundledMap = dataDir.empty() || mppath.substr(0, dataDir.size()) != dataDir;
	if(isBundledMap){
		mapDownloader.mapUploadState.store(2, std::memory_order_release);
	}else{
		mapDownloader.mapUploadState.store(1, std::memory_order_relaxed);
		std::string apiURL = Config::GetInstance().mapapiurl;
		std::atomic<int> * uploadStatePtr     = &mapDownloader.mapUploadState;
		std::atomic<uint32_t> * uploadGenPtr  = &mapDownloader.mapUploadGeneration;
		mapDownloader.mapUploadThread = std::thread([mapname, mppath, apiURL, gen, uploadStatePtr, uploadGenPtr](){
			bool ok = UploadMapToServer(mapname.c_str(), mppath.c_str(), apiURL.c_str());
			if(uploadGenPtr->load(std::memory_order_relaxed) != gen) return;
			uploadStatePtr->store(ok ? 2 : 3, std::memory_order_release);
		});
	}
	world.lobby.creategamestatus = 0;
	ctx.SetCreateGamePending(true);
	std::strncpy(Config::GetInstance().defaultgamename, state.name, sizeof(Config::GetInstance().defaultgamename) - 1);
	Config::GetInstance().defaultgamename[sizeof(Config::GetInstance().defaultgamename) - 1] = '\0';
	Config::GetInstance().Save();
	ctx.PushScreen(MessageModal::Progress("Uploading map..."));
}

bool GameCreatePanelHandleUiIntent(GameCreatePanelState & state,
                                   const silencer::ui::UiAction & action) {
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		if(action.id.empty() || action.id == game_create_panel_detail::kActionOptionsScroll){
			state.optionsScrollDelta += action.amount;
			return true;
		}
		return false;
	}
	if(action.kind == silencer::ui::UiActionKind::SetText){
		if(action.id == game_create_panel_detail::kActionMinLevel){
			game_create_panel_detail::CopyUiText(state.minLevel, static_cast<int>(sizeof(state.minLevel)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionMaxLevel){
			game_create_panel_detail::CopyUiText(state.maxLevel, static_cast<int>(sizeof(state.maxLevel)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionMaxPlayers){
			game_create_panel_detail::CopyUiText(state.maxPlayers, static_cast<int>(sizeof(state.maxPlayers)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionMaxTeams){
			game_create_panel_detail::CopyUiText(state.maxTeams, static_cast<int>(sizeof(state.maxTeams)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionName){
			game_create_panel_detail::CopyUiText(state.name, static_cast<int>(sizeof(state.name)), action.value);
			return true;
		}
		if(action.id == game_create_panel_detail::kActionPassword){
			game_create_panel_detail::CopyUiText(state.password, static_cast<int>(sizeof(state.password)), action.value);
			return true;
		}
		return false;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate){
		if(action.id == game_create_panel_detail::kActionSecurity){
			state.securityClicked = true;
			return true;
		}
		if(action.id == game_create_panel_detail::kActionSpectatable){
			state.spectatableClicked = true;
			return true;
		}
		if(action.id == game_create_panel_detail::kActionCreate){
			state.createClicked = true;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::Select &&
	   game_create_panel_detail::StartsWith(action.id, game_create_panel_detail::kActionMapPrefix)){
		state.mapRowClickedIndex = action.index;
		return true;
	}
	return false;
}

}  // namespace silencer::client_ui::lobby
