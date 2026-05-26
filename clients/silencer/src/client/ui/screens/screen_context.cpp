#include "screen_context.h"

#include "ambience_mixer.h"
#include "audio.h"
#include "config.h"
#include "game.h"
#include "game_state.h"
#include "gasloader.h"
#include "keybinds.h"
#include "map_downloader.h"
#include "renderer.h"
#include "screen.h"
#include "modal.h"
#include "message_modal.h"
#include "surface.h"
#include "runtime/UiInteractionRegistry.h"
#include "lobby.h"
#include "lobbygame.h"
#include "mapfetch.h"
#include "os.h"
#include "peer.h"
#include "renderdevice.h"
#include "runtime/UiActionQueue.h"
#include "team.h"
#include "updater.h"
#include "updaterstage2.h"
#include "user.h"
#include "world.h"

#include <SDL3/SDL_video.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace
{
ScreenContext::UpdateState ToScreenUpdateState(Updater::State state)
{
	switch(state){
		case Updater::IDLE:
			return ScreenContext::UpdateState::Idle;
		case Updater::PROMPTING:
			return ScreenContext::UpdateState::Prompting;
		case Updater::DOWNLOADING:
			return ScreenContext::UpdateState::Downloading;
		case Updater::VERIFYING:
			return ScreenContext::UpdateState::Verifying;
		case Updater::STAGING:
			return ScreenContext::UpdateState::Staging;
		case Updater::FAILED:
			return ScreenContext::UpdateState::Failed;
		case Updater::DONE:
			return ScreenContext::UpdateState::Done;
	}
	assert(false && "Unhandled updater state");
	std::abort();
}

char * CopyOptionalPassword(const char * password, char * buffer, size_t bufferSize)
{
	if(!password) return nullptr;
	if(!buffer || bufferSize == 0) return nullptr;
	std::strncpy(buffer, password, bufferSize - 1);
	buffer[bufferSize - 1] = '\0';
	return buffer;
}

Uint8 ToCreateGameSecurityLevel(Uint8 securityIndex)
{
	switch(securityIndex){
		case 1: return LobbyGame::SECLOW;
		case 2: return LobbyGame::SECMEDIUM;
		case 3: return LobbyGame::SECHIGH;
	}
	return LobbyGame::SECNONE;
}
} // namespace

ScreenContext::ScreenContext(Game & game_,
                             World & world_,
                             Renderer & renderer_,
                             Lobby & lobby_,
                             KeyMap & keymap_,
                             Updater & updater_,
                             AmbienceMixer & ambienceMixer_,
                             MapDownloader & mapDownloader_,
                             SDL_Window * & window_,
                             RenderDevice * & renderdevice_)
    : game(game_),
      renderer(renderer_),
      keymap(keymap_),
      updater(updater_),
      ambienceMixer(ambienceMixer_),
      mapDownloader(mapDownloader_),
      window(window_),
      renderdevice(renderdevice_),
      world(world_),
      lobby(lobby_)
{
}

void ScreenContext::GoToState(Uint8 newState) { game.GoToState(newState); }
bool ScreenContext::GoBack() { return game.GoBack(); }
void ScreenContext::RequestQuit() { game.quitRequested = true; }
void ScreenContext::LeaveJoinedGame() { game.LeaveJoinedGame(); }
bool ScreenContext::IsJoiningGame() const { return game.joininggame; }
void ScreenContext::SetJoiningGame(bool joining) { game.joininggame = joining; }
ScreenContext::JoiningGameResult ScreenContext::ConsumeJoiningGameResult() {
	if(!game.joininggame) return JoiningGameResult::Pending;
	if(world.IsConnected()){
		game.joininggame = false;
		return JoiningGameResult::Connected;
	}
	if(world.IsIdle()){
		game.joininggame = false;
		return JoiningGameResult::Failed;
	}
	return JoiningGameResult::Pending;
}
bool ScreenContext::HandleLobbyDisconnect() {
	if(world.lobby.state != Lobby::DISCONNECTED) return false;
	world.Disconnect();
	game.GoToState(GameState::LOBBYCONNECT);
	return true;
}
bool ScreenContext::IsCreateGamePending() const { return game.creategameclicked; }
void ScreenContext::SetCreateGamePending(bool pending) { game.creategameclicked = pending; }
void ScreenContext::StartCreateGameRequest() {
	world.lobby.creategamestatus = 0;
	game.creategameclicked = true;
}
ScreenContext::CreateGameDefaults ScreenContext::CurrentCreateGameDefaults() const {
	CreateGameDefaults defaults;
	defaults.name = Config::GetInstance().defaultgamename;
	defaults.spectatable = Config::GetInstance().lastspectatable;
	return defaults;
}
void ScreenContext::SetCreateGameSpectatableDefault(bool spectatable) {
	Config::GetInstance().lastspectatable = spectatable;
	Config::GetInstance().Save();
}
void ScreenContext::SaveDefaultCreateGameName(const char * name) {
	std::strncpy(Config::GetInstance().defaultgamename,
	             name,
	             sizeof(Config::GetInstance().defaultgamename) - 1);
	Config::GetInstance().defaultgamename[
		sizeof(Config::GetInstance().defaultgamename) - 1] = '\0';
	Config::GetInstance().Save();
}
ScreenContext::CreateLobbyGameResult ScreenContext::ConsumeCreateLobbyGameResult() {
	if(!game.creategameclicked) return CreateLobbyGameResult::Pending;
	if(world.lobby.creategamestatus == 1){
		world.lobby.creategamestatus = 0;
		game.creategameclicked = false;
		LobbyGame * lobbyGame = world.lobby.GetGameById(world.lobby.createdgameid);
		if(lobbyGame){
			world.SeedGameInfoFromLobbyGame(*lobbyGame);
			JoinLobbyGame(*lobbyGame, lobbyGame->password);
			LoadLobbyGameMapData(*lobbyGame);
		}
		return CreateLobbyGameResult::Created;
	}
	if(world.lobby.creategamestatus != 100 && world.lobby.creategamestatus != 0){
		world.lobby.creategamestatus = 0;
		game.creategameclicked = false;
		return CreateLobbyGameResult::Failed;
	}
	return CreateLobbyGameResult::Pending;
}
LobbyGame * ScreenContext::FindLobbyGame(Uint32 gameId) const { return world.lobby.GetGameById(gameId); }
LobbyGame * ScreenContext::CurrentLobbyGame() const { return FindLobbyGame(game.currentlobbygameid); }
void ScreenContext::JoinLobbyGame(LobbyGame & lobbyGame, char * password) {
	game.currentlobbygameid = lobbyGame.id;
	game.JoinGame(lobbyGame, password);
}
void ScreenContext::SpectateLobbyGame(LobbyGame & lobbyGame, char * password) {
	game.currentlobbygameid = lobbyGame.id;
	game.SpectateGame(lobbyGame, password);
}
bool ScreenContext::JoinLobbyGameById(Uint32 gameId, const char * password) {
	LobbyGame * lobbyGame = FindLobbyGame(gameId);
	if(!lobbyGame) return false;
	char passwordBuffer[64];
	JoinLobbyGame(*lobbyGame,
	              CopyOptionalPassword(password, passwordBuffer, sizeof(passwordBuffer)));
	return true;
}
bool ScreenContext::SpectateLobbyGameById(Uint32 gameId, const char * password) {
	LobbyGame * lobbyGame = FindLobbyGame(gameId);
	if(!lobbyGame) return false;
	char passwordBuffer[64];
	SpectateLobbyGame(*lobbyGame,
	                  CopyOptionalPassword(password, passwordBuffer, sizeof(passwordBuffer)));
	return true;
}
SDL_GamepadType ScreenContext::CurrentGamepadType() const {
	SDL_Gamepad * pad = game.GetGamepad();
	return pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
}
bool ScreenContext::PushScreen(std::unique_ptr<Screen> s) { return game.PushScreen(std::move(s)); }
bool ScreenContext::PopScreen() { return game.PopScreen(); }
bool ScreenContext::ReplaceScreen(std::unique_ptr<Screen> s) { return game.ReplaceScreen(std::move(s)); }
Screen * ScreenContext::TopScreen() const { return game.GetTopScreen(); }
bool ScreenContext::ShowModal(std::unique_ptr<Modal> m) {
	return game.PushScreen(std::unique_ptr<Screen>(static_cast<Screen *>(m.release())));
}

bool ScreenContext::ShowMessage(const char * msg, std::function<void()> onClose) {
	return game.PushScreen(std::make_unique<MessageModal>(msg ? msg : "", std::move(onClose)));
}

void ScreenContext::ClearUiFocus() {
	game.UiInteractions().ClearFocus();
}

void ScreenContext::PlayUiClickSound() {
	Audio & audio = Audio::GetInstance();
	if(!audio.enabled) return;
	const std::string & sound = GASLoader::Get().player.soundUIClick;
	auto it = world.resources.soundbank.find(sound);
	if(it == world.resources.soundbank.end() || !it->second) return;
	audio.PlayUI(it->second);
}

std::string ScreenContext::KeybindPresetText() const {
	if(!keymap.label.empty()) return keymap.label;
	if(!keymap.name.empty()) return keymap.name;
	return Config::GetInstance().active_keybind_profile;
}

void ScreenContext::CycleKeybindPreset() {
	::CycleKeybindPreset(keymap);
}

void ScreenContext::SaveActiveKeybindProfileIfCustom() {
	const std::string active = Config::GetInstance().active_keybind_profile;
	if(active == "default" || active == "wasd" || active == "gamepad") return;
	keymap.SaveFile(WritableProfilePath(active));
}

void ScreenContext::ReloadActiveKeymap() {
	LoadActiveKeymap(keymap);
}

ScreenContext::LegacyKeyBindingSlots ScreenContext::LegacyKeyBinding(Action action) const {
	LegacyKeyBindingSlots out;
	const auto & ab = keymap.Get(action);
	if(ab.bindings.empty()) return out;
	const auto & b0 = ab.bindings[0];
	if(b0.keys.size() >= 2 &&
	   b0.keys[0].device == BindingDevice::Keyboard &&
	   b0.keys[1].device == BindingDevice::Keyboard){
		out.key1 = static_cast<SDL_Scancode>(b0.keys[0].code);
		out.key2 = static_cast<SDL_Scancode>(b0.keys[1].code);
		out.and_ = true;
		return out;
	}
	if(!b0.keys.empty() && b0.keys[0].device == BindingDevice::Keyboard){
		out.key1 = static_cast<SDL_Scancode>(b0.keys[0].code);
	}
	if(ab.bindings.size() >= 2){
		const auto & b1 = ab.bindings[1];
		if(!b1.keys.empty() && b1.keys[0].device == BindingDevice::Keyboard){
			out.key2 = static_cast<SDL_Scancode>(b1.keys[0].code);
		}
	}
	return out;
}

void ScreenContext::WriteLegacyKeyBinding(Action action,
                                          SDL_Scancode key1,
                                          SDL_Scancode key2,
                                          bool and_) {
	ForkActiveProfileIfBuiltin(keymap);
	auto & ab = keymap.Get(action);
	ab.bindings.clear();
	auto mk = [](SDL_Scancode sc){
		BindingKey k;
		k.device  = BindingDevice::Keyboard;
		k.code    = static_cast<int>(sc);
		k.axisDir = 0;
		return k;
	};
	if(key1 == SDL_SCANCODE_UNKNOWN && key2 == SDL_SCANCODE_UNKNOWN) return;
	if(and_ && key1 != SDL_SCANCODE_UNKNOWN && key2 != SDL_SCANCODE_UNKNOWN){
		Binding b;
		b.keys.push_back(mk(key1));
		b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
		return;
	}
	if(key1 != SDL_SCANCODE_UNKNOWN){
		Binding b;
		b.keys.push_back(mk(key1));
		ab.bindings.push_back(std::move(b));
	}
	if(key2 != SDL_SCANCODE_UNKNOWN){
		Binding b;
		b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
	}
}

std::string ScreenContext::KeyBindingSlotLabel(Action action, int slot) const {
	const auto & ab = keymap.Get(action);
	int found = 0;
	for(const auto & b : ab.bindings){
		if(b.keys.empty()) continue;
		if(found == slot){
			const auto & k = b.keys[0];
			if(k.device == BindingDevice::Keyboard){
				return KeyMap::GetKeyName(static_cast<SDL_Scancode>(k.code));
			}
			std::string s = Stringify(k);
			auto colon = s.find(':');
			std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
			return GamepadShortLabel(raw, CurrentGamepadType());
		}
		found++;
	}
	return KeyMap::GetKeyName(SDL_SCANCODE_UNKNOWN);
}

bool ScreenContext::SetCapturedBinding(Action action, int slot, const silencer::ui::UiBindingInput & input) {
	BindingKey bindingKey{};
	if(input.kind == silencer::ui::UiBindingInputKind::GamepadButtonDown){
		bindingKey.device = BindingDevice::GamepadButton;
		bindingKey.code = input.code;
		bindingKey.axisDir = 0;
	}else if(input.kind == silencer::ui::UiBindingInputKind::GamepadAxisMoved){
		bindingKey.device = BindingDevice::GamepadAxis;
		bindingKey.code = input.code;
		bindingKey.axisDir = static_cast<int8_t>(input.axisDir < 0 ? -1 : 1);
	}else{
		return false;
	}

	ForkActiveProfileIfBuiltin(keymap);
	auto & ab = keymap.Get(action);
	Binding binding;
	binding.keys.push_back(bindingKey);
	if(slot == 0){
		if(ab.bindings.empty()) ab.bindings.push_back(binding);
		else ab.bindings[0] = binding;
	}else{
		if(ab.bindings.empty()) ab.bindings.push_back(Binding{});
		if(ab.bindings.size() < 2) ab.bindings.push_back(binding);
		else ab.bindings[1] = binding;
	}
	return true;
}

void ScreenContext::ResetPresentation(int paletteIdx) {
	renderer.palette.SetPalette(paletteIdx);
	game.GetScreenBuffer().Clear(0);
	game.gameRenderer.SetColors(renderer.palette.GetColors());
}

void ScreenContext::ResetMenuPresentation(int paletteIdx) {
	ResetPresentation(paletteIdx);
	renderer.camera.SetPosition(320, 240);
}

bool ScreenContext::UiBlinkVisible() const {
	return (renderer.GetHudAnimationPhase() % 32) < 16;
}

std::string ScreenContext::ClientVersion() const {
	return world.GetVersion();
}

ScreenContext::UiSpriteFrameMetrics
ScreenContext::GetUiSpriteFrameMetrics(Uint8 bank, Uint16 index) const {
	const Resources & resources = world.resources;
	UiSpriteFrameMetrics metrics;
	metrics.offsetX = resources.spriteoffsetx[bank][index];
	metrics.offsetY = resources.spriteoffsety[bank][index];
	metrics.width = static_cast<int>(resources.spritewidth[bank][index]);
	metrics.height = static_cast<int>(resources.spriteheight[bank][index]);
	return metrics;
}

Uint32 ScreenContext::FrameCount() const {
	return static_cast<Uint32>(game.GetFrameCount());
}

Uint32 ScreenContext::WorldTickCount() const {
	return world.tickcount;
}

bool ScreenContext::LobbyMusicFadedIn() const {
	return ambienceMixer.FadedIn();
}

std::string ScreenContext::LobbyGameChannelName(LobbyGame & lobbyGame) {
	char name[256];
	ambienceMixer.GetGameChannelName(lobbyGame, name);
	return name;
}

std::vector<std::string> ScreenContext::CreateGameMapLabels() {
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
	mapDownloader.servermaps.clear();
	for(auto & entry : FetchServerMapList(Config::GetInstance().mapapiurl)){
		if(std::find(maps.begin(), maps.end(), entry.first) == maps.end()){
			std::string label = "[DL] " + entry.first;
			maps.push_back(label);
			mapDownloader.servermaps[label] = entry.second;
		}
	}
	return maps;
}

void ScreenContext::SelectCreateGameMap(int mapIndex) {
	mapDownloader.selectedmap = mapIndex;
}

bool ScreenContext::IsServerMapLabel(const std::string & mapLabel) const {
	return mapDownloader.servermaps.count(mapLabel) > 0;
}

std::string ScreenContext::FindMapPath(const char * mapName) {
	return mapDownloader.FindMap(mapName);
}

void ScreenContext::LoadLobbyGameMapData(LobbyGame & lobbyGame) {
	mapDownloader.LoadMapData(
		mapDownloader.FindMap(lobbyGame.mapname, &lobbyGame.maphash).c_str());
}

ScreenContext::CreateGameMapUploadResult
ScreenContext::ConsumeCreateGameMapUploadResult() {
	int uploadState = mapDownloader.mapUploadState.load(std::memory_order_acquire);
	if(uploadState == 2){
		mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
		const char * password =
			mapDownloader.pendingCreate.password.empty()
				? nullptr
				: mapDownloader.pendingCreate.password.c_str();
		world.lobby.CreateGame(
			mapDownloader.pendingCreate.gamename.c_str(),
			mapDownloader.pendingCreate.mapname.c_str(),
			mapDownloader.pendingCreate.maphash,
			password,
			mapDownloader.pendingCreate.securitylevel,
			mapDownloader.pendingCreate.minlevel,
			mapDownloader.pendingCreate.maxlevel,
			mapDownloader.pendingCreate.maxplayers,
			mapDownloader.pendingCreate.maxteams,
			mapDownloader.pendingCreate.spectatable);
		return CreateGameMapUploadResult::SubmittedCreateGame;
	}
	if(uploadState == 3){
		mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
		return CreateGameMapUploadResult::Failed;
	}
	return CreateGameMapUploadResult::Idle;
}

std::string ScreenContext::CreateGameProgressText() const {
	std::string text =
		(mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 1)
			? "Uploading map"
			: "Creating game";
	int dots = (world.tickcount / 4) % 6;
	if(dots > 3) dots = 6 - dots;
	for(int i = 0; i < dots; i++) text += ".";
	return text;
}

bool ScreenContext::CreateGameMapUploadIdle() const {
	return mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 0;
}

bool ScreenContext::ShouldDismissCreateGameProgress() const {
	return world.lobby.creategamestatus != 100 && CreateGameMapUploadIdle() &&
	       (world.IsConnected() || world.IsIdle());
}

bool ScreenContext::BeginConnectedLobbyGame() {
	if(!world.GetPeer(world.GetLocalPeerId())) return false;
	ResetJoinMapDownload();
	const Uint8 agency = world.lobby.GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
	world.SetTech(Config::GetInstance().defaulttechchoices[agency]);
	return true;
}

std::string ScreenContext::JoinCurrentLobbyGameChannel() {
	LobbyGame * lobbyGame = CurrentLobbyGame();
	if(!lobbyGame) return std::string();
	const std::string channel = LobbyGameChannelName(*lobbyGame);
	strcpy(world.lobby.lastchannel, world.lobby.channel);
	world.lobby.JoinChannel(channel.c_str());
	return lobbyGame->mapname;
}

void ScreenContext::RequestLobbyGameListRefresh() {
	world.lobby.gamesprocessed = false;
}

bool ScreenContext::ConsumeLobbyGameListRefresh() {
	if(world.lobby.gamesprocessed) return false;
	world.lobby.gamesprocessed = true;
	return true;
}

std::vector<ScreenContext::LobbyGameListRow>
ScreenContext::LobbyGameListRows() const {
	std::vector<LobbyGameListRow> rows;
	rows.reserve(world.lobby.games.size());
	for(LobbyGame * lobbyGame : world.lobby.games){
		if(!lobbyGame) continue;
		LobbyGameListRow row;
		row.gameId = lobbyGame->id;
		row.name = lobbyGame->name;
		rows.push_back(std::move(row));
	}
	return rows;
}

ScreenContext::LobbyGameDetails
ScreenContext::LobbyGameDetailsFor(Uint32 gameId) const {
	LobbyGameDetails details;
	LobbyGame * lobbyGame = world.lobby.GetGameById(gameId);
	if(!lobbyGame) return details;

	details.found = true;
	details.gameId = lobbyGame->id;
	details.name = lobbyGame->name;
	details.mapName = lobbyGame->mapname;
	User * creator = world.lobby.GetUserInfo(lobbyGame->accountid);
	if(creator) details.creatorName = creator->name;
	switch(lobbyGame->securitylevel){
		case LobbyGame::SECLOW:
			details.securityLevel = LobbyGameSecurityLevel::Low;
			break;
		case LobbyGame::SECMEDIUM:
			details.securityLevel = LobbyGameSecurityLevel::Medium;
			break;
		case LobbyGame::SECHIGH:
			details.securityLevel = LobbyGameSecurityLevel::High;
			break;
		default:
			details.securityLevel = LobbyGameSecurityLevel::None;
			break;
	}
	details.passwordProtected = std::strlen(lobbyGame->password) > 0;
	details.passwordRequiredForLocalAccount =
		details.passwordProtected && lobbyGame->accountid != world.lobby.accountid;
	details.inGame = lobbyGame->state == 1;
	details.canRejoin = lobbyGame->canrejoin;
	details.spectatable = lobbyGame->spectatable;
	details.minLevel = lobbyGame->minlevel;
	details.maxLevel = lobbyGame->maxlevel;
	details.players = lobbyGame->players;
	details.maxPlayers = lobbyGame->maxplayers;
	details.maxTeams = lobbyGame->maxteams;
	return details;
}

ScreenContext::LocalLobbyAgencyLevel
ScreenContext::CurrentLobbyAgencyLevel() const {
	LocalLobbyAgencyLevel result;
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(!user) return result;
	const Uint8 agency =
		world.lobby.GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
	result.found = true;
	result.level = user->agency[agency].level;
	return result;
}

Uint8 ScreenContext::DefaultLobbyAgency() const {
	return Config::GetInstance().defaultagency;
}

Uint8 ScreenContext::SelectedLobbyAgency() const {
	return world.lobby.GetSelectedAgencyOrDefault(DefaultLobbyAgency());
}

void ScreenContext::SetLobbyAgency(Uint8 agency) {
	world.SetAgency(agency);
}

ScreenContext::LobbyCharacterStats
ScreenContext::LobbyCharacterStatsForAgency(Uint8 agency) const {
	LobbyCharacterStats result;
	const Lobby::Character * character = world.lobby.GetSelectedCharacter();
	result.name = character ? character->name : "No Agent";

	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(!user || user->retrieving) return result;

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

ScreenContext::LobbyChannelChange ScreenContext::ConsumeLobbyChannelChange() {
	LobbyChannelChange result;
	if(!world.lobby.channelchanged) return result;
	if(world.lobby.lastchannel[0] == '\0'){
		std::strcpy(world.lobby.lastchannel, world.lobby.channel);
	}
	result.changed = true;
	result.channel = world.lobby.channel;
	world.lobby.channelchanged = false;
	return result;
}

bool ScreenContext::ConsumeLobbyPresenceRefresh() {
	const bool refresh = world.lobby.presencechanged || !world.lobby.gamesprocessed;
	world.lobby.presencechanged = false;
	return refresh;
}

std::vector<ScreenContext::LobbyPresenceRow>
ScreenContext::LobbyPresenceRows() const {
	std::vector<LobbyPresenceRow> rows;
	rows.reserve(world.lobby.presence.size());
	for(const auto & kv : world.lobby.presence){
		const Lobby::PresenceEntry & entry = kv.second;
		LobbyPresenceRow row;
		row.label = entry.name;
		row.group = (entry.status <= 2) ? entry.status : 0;
		if(entry.gameid != 0){
			LobbyGame * lobbyGame = world.lobby.GetGameById(entry.gameid);
			if(lobbyGame){
				row.label += " [";
				row.label += lobbyGame->name;
				row.label += "]";
			}
		}
		rows.push_back(std::move(row));
	}
	return rows;
}

std::vector<ScreenContext::LobbyChatMessage>
ScreenContext::DrainLobbyChatMessages() {
	std::vector<LobbyChatMessage> messages;
	messages.reserve(world.lobby.chatmessages.size());
	while(!world.lobby.chatmessages.empty()){
		auto message = world.lobby.chatmessages.front();
		const char * text = message.data();
		const size_t length = std::strlen(text);
		LobbyChatMessage chatMessage;
		chatMessage.text = text ? std::string(text) : std::string();
		chatMessage.color = static_cast<Uint8>(message[length + 1]);
		chatMessage.brightness = static_cast<Uint8>(message[length + 2]);
		messages.push_back(std::move(chatMessage));
		world.lobby.chatmessages.pop_front();
	}
	return messages;
}

void ScreenContext::SendLobbyChat(const char * message) {
	if(!message || message[0] == '\0') return;
	world.lobby.SendChat(world.lobby.channel, message);
}

bool ScreenContext::LobbyJoinReadyBlocked() {
	return world.IsLocalHostWaitingForMapDownloads();
}

std::vector<ScreenContext::LobbyJoinRosterRow>
ScreenContext::LobbyJoinRosterRows() const {
	std::vector<LobbyJoinRosterRow> rows;
	if(!world.IsConnected()) return rows;

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

			LobbyJoinRosterRow row;
			row.ready = peer->isready;
			row.agency = team->agency;
			row.teamNumber = team->number;
			row.peerSlot = static_cast<Uint8>(i);
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

ScreenContext::LobbyTechSnapshot ScreenContext::CurrentLobbyTechSnapshot() {
	LobbyTechSnapshot snapshot;
	const Uint8 localId = world.GetLocalPeerId();
	Peer * localPeer = world.GetPeer(localId);
	Team * team = world.GetPeerTeam(localId);

	int localTechSlotsLeft = 0;
	if(localPeer && team){
		User * user = world.lobby.GetUserInfo(localPeer->accountid);
		if(user){
			localTechSlotsLeft =
				user->agency[team->agency].techslots - world.TechSlotsUsed(*localPeer);
			snapshot.slotsLeft =
				"Tech slots left: " + std::to_string(localTechSlotsLeft);
		}
	}else if(!localPeer && world.tickcount % 12 == 0){
		world.peers.RequestPeerList();
	}

	if(!team) return snapshot;

	int peerIndex = 0;
	for(int i = 0; i < 4 && peerIndex < 3; i++){
		if(team->peers[i] == localId) continue;
		if(i >= team->numpeers){ peerIndex++; continue; }
		Peer * peer = world.GetPeer(team->peers[i]);
		User * user = peer ? world.lobby.GetUserInfo(peer->accountid) : nullptr;
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
	for(size_t itemIndex = 0; itemIndex < world.buyableitems.size(); itemIndex++){
		BuyableItem * item = world.buyableitems[itemIndex];
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
			Peer * peer = world.GetPeer(team->peers[cols[col].peerSlot]);
			const bool selected = peer && (peer->techchoices & item->techchoice);
			bool interactable = false;
			if(cols[col].isLocal){
				interactable =
					(item->techslots <= localTechSlotsLeft) || selected;
			}
			const Uint8 brightness = cols[col].isLocal && interactable ? 128 : 64;
			row.cells[col].draw = true;
			row.cells[col].selected = selected;
			row.cells[col].brightness = brightness;
			if(cols[col].isLocal) row.labelBrightness = brightness;
		}

		snapshot.rows.push_back(std::move(row));
	}

	return snapshot;
}

ScreenContext::LobbyTechItemDetails
ScreenContext::LobbyTechItemDetailsForIndex(int itemIndex) const {
	LobbyTechItemDetails details;
	if(itemIndex < 0 || itemIndex >= static_cast<int>(world.buyableitems.size())){
		return details;
	}
	BuyableItem * item = world.buyableitems[itemIndex];
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

void ScreenContext::ToggleLobbyTechChoice(int itemIndex) {
	const Uint8 localId = world.GetLocalPeerId();
	Peer * localPeer = world.GetPeer(localId);
	Team * team = world.GetPeerTeam(localId);
	if(!localPeer || !team || itemIndex < 0
	   || itemIndex >= static_cast<int>(world.buyableitems.size())){
		return;
	}

	BuyableItem * item = world.buyableitems[itemIndex];
	if(!item) return;
	User * user = world.lobby.GetUserInfo(localPeer->accountid);
	if(!user) return;

	const int techSlotsLeft =
		user->agency[team->agency].techslots - world.TechSlotsUsed(*localPeer);
	const bool selected = (localPeer->techchoices & item->techchoice) != 0;
	const bool interactable = (item->techslots <= techSlotsLeft) || selected;
	if(!interactable) return;

	const Uint32 newChoices = localPeer->techchoices ^ item->techchoice;
	world.SetTech(newChoices);
	Config::GetInstance().defaulttechchoices[team->agency] = newChoices;
	Config::GetInstance().Save();
}

void ScreenContext::BeginCreateGameMapUpload(const std::string & gameName,
                                             const std::string & mapName,
                                             const std::string & password,
                                             Uint8 securityIndex,
                                             Uint8 minLevel,
                                             Uint8 maxLevel,
                                             Uint8 maxPlayers,
                                             Uint8 maxTeams,
                                             bool spectatable) {
	unsigned char mapHash[20];
	const std::string mapPath = mapDownloader.FindMap(mapName.c_str());
	mapDownloader.CalculateMapHash(mapPath.c_str(), &mapHash);

	auto & pending = mapDownloader.pendingCreate;
	pending.gamename      = gameName;
	pending.mapname       = mapName;
	pending.password      = password;
	std::memcpy(pending.maphash, mapHash, 20);
	pending.securitylevel = ToCreateGameSecurityLevel(securityIndex);
	pending.minlevel      = minLevel;
	pending.maxlevel      = maxLevel;
	pending.maxplayers    = maxPlayers;
	pending.maxteams      = maxTeams;
	pending.spectatable   = spectatable;

	if(mapDownloader.mapUploadThread.joinable()) mapDownloader.mapUploadThread.detach();
	uint32_t generation = ++mapDownloader.mapUploadGeneration;
	std::string dataDir = GetDataDir();
	bool isBundledMap = dataDir.empty() || mapPath.substr(0, dataDir.size()) != dataDir;
	if(isBundledMap){
		mapDownloader.mapUploadState.store(2, std::memory_order_release);
	}else{
		mapDownloader.mapUploadState.store(1, std::memory_order_relaxed);
		std::string apiURL = Config::GetInstance().mapapiurl;
		std::atomic<int> * uploadStatePtr = &mapDownloader.mapUploadState;
		std::atomic<uint32_t> * uploadGenerationPtr = &mapDownloader.mapUploadGeneration;
		mapDownloader.mapUploadThread =
			std::thread([mapName, mapPath, apiURL, generation, uploadStatePtr, uploadGenerationPtr](){
				bool ok = UploadMapToServer(mapName.c_str(), mapPath.c_str(), apiURL.c_str());
				if(uploadGenerationPtr->load(std::memory_order_relaxed) != generation) return;
				uploadStatePtr->store(ok ? 2 : 3, std::memory_order_release);
			});
	}
}

void ScreenContext::ResetJoinMapDownload() {
	mapDownloader.mapexistchecked = false;
	mapDownloader.mapjoingeneration.fetch_add(1, std::memory_order_relaxed);
	mapDownloader.mapjoinstate.store(0, std::memory_order_relaxed);
	if(mapDownloader.mapjointhread.joinable()) mapDownloader.mapjointhread.detach();
}

void ScreenContext::PumpMapDownload() {
	mapDownloader.ProcessMapDownload();
}

bool ScreenContext::LobbyNetworkIdle() const {
	return world.IsIdle();
}

bool ScreenContext::LobbyNetworkConnected() const {
	return world.IsConnected();
}

bool ScreenContext::JoinedGameDisconnected() const {
	return !world.IsConnected();
}

void ScreenContext::PresentUpdate(const std::string & url, const uint8_t sha256[32]) {
	updater.PresentUpdate(url, sha256);
}

ScreenContext::UpdateState ScreenContext::CurrentUpdateState() {
	return ToScreenUpdateState(updater.GetState());
}

float ScreenContext::UpdateProgress() {
	return updater.GetProgress();
}

std::string ScreenContext::UpdateErrorMessage() {
	return updater.GetErrorMessage();
}

int ScreenContext::UpdateRetryCount() {
	return updater.GetRetryCount();
}

void ScreenContext::ConsentUpdate() {
	updater.Consent();
}

void ScreenContext::CancelUpdate() {
	updater.Cancel();
}

void ScreenContext::RetryUpdate() {
	updater.Retry();
}

void ScreenContext::OpenUpdateDownloadPage() {
	std::string url = updater.GetDownloadURL();
#ifdef _WIN32
	std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
	std::string cmd = "open '" + url + "'";
#else
	std::string cmd = "xdg-open '" + url + "' &";
#endif
	system(cmd.c_str());
}

bool ScreenContext::LaunchStagedUpdate() {
	std::string zippath =
#ifdef _WIN32
		std::string(getenv("TEMP") ? getenv("TEMP") : ".") + "\\silencer-update.zip";
#else
		"/tmp/silencer-update.zip";
#endif
	fprintf(stderr, "[updater] UpdateScreen invoking UpdaterStage2::Launch with zip=%s\n",
	        zippath.c_str());
	if(UpdaterStage2::Launch(zippath)){
		updater.MarkStage2Spawned();
		return true;
	}
	fprintf(stderr, "[updater] UpdaterStage2::Launch failed; returning to main menu\n");
	return false;
}

void ScreenContext::SetWindowFullscreen(bool fullscreen) {
	if(window) SDL_SetWindowFullscreen(window, fullscreen);
}

void ScreenContext::SetScaleFilter(bool enabled) {
	if(renderdevice) renderdevice->SetScaleFilter(enabled);
}

void ScreenContext::BeginLobbyPanelBorderBlur(int width, int height, float uiScale) {
	if(renderdevice) renderdevice->BeginLobbyPanelBorderBlur(width, height, uiScale);
}

void ScreenContext::AddLobbyPanelBorderBlurRect(int x, int y, int w, int h) {
	if(!renderdevice || w <= 0 || h <= 0) return;
	renderdevice->AddLobbyPanelBorderBlurRect(SDL_Rect{ x, y, w, h });
}
