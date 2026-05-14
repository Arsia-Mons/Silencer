#include "game_create_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/scroll_list.h"
#include "primitives/text_input.h"

#include "lobby_screen.h"
#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "lobbygame.h"
#include "screen.h"
#include "config.h"
#include "os.h"
#include "resources.h"
#include "map_downloader.h"
#include "mapfetch.h"
#include "message_modal.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::ScrollList;
using silencer::ui::primitives::ScrollListHandle;
using silencer::ui::primitives::ScrollListOpts;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;

namespace silencer::client_ui::lobby {

namespace {

// Legacy on-screen coords kept ONLY for inspector hit-rect registration.
constexpr int    kYSpace       = 14;
constexpr int    kRowHeight    = 14;
constexpr int    kValueX       = 323;
constexpr int    kFormLeft     = 243;
constexpr int    kFormTop      = 87;
constexpr int    kMapListX     = 407;
constexpr int    kMapListY     = 89;
constexpr Uint16 kMapListW     = 214;
constexpr Uint16 kMapListH     = 265;
constexpr Uint8  kMapListLineH = 14;
constexpr Uint8  kScrollbarBank = 7;
constexpr int    kNameInputX = 410, kNameInputY = 375;
constexpr Uint16 kNameInputW = 210, kNameInputH = 14;
constexpr int    kPwInputX   = 410, kPwInputY   = 405;
constexpr Uint16 kPwInputW   = 210, kPwInputH   = 14;
constexpr int    kCreateBtnX = 436, kCreateBtnY = 430;

constexpr uint16_t kPanelPad        = 6;
constexpr uint16_t kFormRowH        = 14;
constexpr uint16_t kFormRowGap      = 3;
constexpr uint16_t kFormColumnGap   = 6;
constexpr uint16_t kTallSectionGap  = 4;

constexpr int kMaxMapRows = 1024;
Clay_String g_mapSlab[kMaxMapRows];
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

Clay_String FromCStr(const char * s) {
	return Clay_String{ false, static_cast<int32_t>(strlen(s)), s };
}

Clay_String StaticId(const char * s) {
	return Clay_String{ true, static_cast<int32_t>(strlen(s)), s };
}

const char * SecurityLabel(Uint8 idx) {
	switch(idx){
		case 0:  return "Off";
		case 1:  return "Low";
		case 3:  return "High";
		default: return "Medium";
	}
}

bool StartsWith(const std::string & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

void CopyUiText(char * dst, int dstLen, const std::string & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
}

void RegisterButton(const char * label, const char * actionId,
                    int x, int y, int w, int h, bool selected = false) {
	silencer::ui::automation::Widget reg;
	reg.id        = actionId;
	reg.labelText = label;
	reg.kind      = silencer::ui::automation::WidgetKind::Button;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	reg.selected  = selected;
	silencer::ui::automation::Register(reg);
}

void RegisterTextInput(const char * label, const char * actionId,
                       int x, int y, int w, int h,
                       char * buf, int cap, bool isPassword = false) {
	silencer::ui::automation::Widget reg;
	reg.id            = actionId;
	reg.labelText     = label;
	reg.kind          = silencer::ui::automation::WidgetKind::TextInput;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	reg.value         = buf ? buf : "";
	reg.maxLength     = cap > 0 ? cap - 1 : 0;
	reg.isPassword    = isPassword;
	silencer::ui::automation::Register(reg);
}

void RegisterListRow(const char * label, const std::string & actionId,
                     int x, int y, int w, int h,
                     int index, bool selected) {
	silencer::ui::automation::Widget reg;
	reg.id         = actionId;
	reg.labelText  = label ? label : "";
	reg.kind       = silencer::ui::automation::WidgetKind::ListRow;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	reg.index      = index;
	reg.selected   = selected;
	silencer::ui::automation::Register(reg);
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

}  // namespace

void GameCreatePanelInit(GameCreatePanelState & state, ScreenContext & ctx) {
	state = GameCreatePanelState{};
	state.spectatable = Config::GetInstance().lastspectatable;
	std::strncpy(state.name, Config::GetInstance().defaultgamename, sizeof(state.name) - 1);
	state.name[sizeof(state.name) - 1] = '\0';
	BuildMapList(state, ctx);
	ctx.mapDownloader.selectedmap = -1;
	ctx.game.creategameclicked = false;
}

void GameCreatePanelTick(GameCreatePanelState & state,
                         World & world,
                         ScreenContext & ctx,
                         LobbyScreen & owner) {
	MapDownloader & mapDownloader = ctx.mapDownloader;
	Game & game = ctx.game;

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
		game.creategameclicked = false;
		Screen * top = game.GetTopScreen();
		MessageModal * m = dynamic_cast<MessageModal *>(top);
		if(m && m->IsProgress()) ctx.PopScreen();
		ctx.ShowMessage("Could not upload map");
	}
	if(world.lobby.creategamestatus == 1 && game.creategameclicked){
		world.lobby.creategamestatus = 0;
		game.creategameclicked = false;
		LobbyGame * lobbygame = world.lobby.GetGameById(world.lobby.createdgameid);
		if(lobbygame){
			owner.SeedHostGameInfo(world, *lobbygame);
			game.JoinGame(*lobbygame, lobbygame->password);
			mapDownloader.LoadMapData(mapDownloader.FindMap(lobbygame->mapname, &lobbygame->maphash).c_str());
			game.currentlobbygameid = lobbygame->id;
		}
	}else if(world.lobby.creategamestatus != 100 && world.lobby.creategamestatus != 0 && game.creategameclicked){
		world.lobby.creategamestatus = 0;
		game.creategameclicked = false;
		Screen * top = game.GetTopScreen();
		MessageModal * m = dynamic_cast<MessageModal *>(top);
		if(m && m->IsProgress()) ctx.PopScreen();
		ctx.ShowMessage("Could not create game");
	}

	if(!state.createClicked) return;
	state.createClicked = false;
	if(game.creategameclicked) return;

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
	game.creategameclicked = true;
	std::strncpy(Config::GetInstance().defaultgamename, state.name, sizeof(Config::GetInstance().defaultgamename) - 1);
	Config::GetInstance().defaultgamename[sizeof(Config::GetInstance().defaultgamename) - 1] = '\0';
	Config::GetInstance().Save();
	ctx.PushScreen(MessageModal::Progress("Uploading map..."));
}

bool GameCreatePanelHandleUiIntent(GameCreatePanelState & state,
                                   const silencer::ui::UiAction & action) {
	if(action.kind == silencer::ui::UiActionKind::SetText){
		if(action.id == kActionMinLevel){
			CopyUiText(state.minLevel, static_cast<int>(sizeof(state.minLevel)), action.value);
			return true;
		}
		if(action.id == kActionMaxLevel){
			CopyUiText(state.maxLevel, static_cast<int>(sizeof(state.maxLevel)), action.value);
			return true;
		}
		if(action.id == kActionMaxPlayers){
			CopyUiText(state.maxPlayers, static_cast<int>(sizeof(state.maxPlayers)), action.value);
			return true;
		}
		if(action.id == kActionMaxTeams){
			CopyUiText(state.maxTeams, static_cast<int>(sizeof(state.maxTeams)), action.value);
			return true;
		}
		if(action.id == kActionName){
			CopyUiText(state.name, static_cast<int>(sizeof(state.name)), action.value);
			return true;
		}
		if(action.id == kActionPassword){
			CopyUiText(state.password, static_cast<int>(sizeof(state.password)), action.value);
			return true;
		}
		return false;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate){
		if(action.id == kActionSecurity){
			state.securityClicked = true;
			return true;
		}
		if(action.id == kActionSpectatable){
			state.spectatableClicked = true;
			return true;
		}
		if(action.id == kActionCreate){
			state.createClicked = true;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::Select &&
	   StartsWith(action.id, kActionMapPrefix)){
		state.mapRowClickedIndex = action.index;
		return true;
	}
	return false;
}

void BuildGameCreateUpperTree(GameCreatePanelState & state,
                              Resources & resources) {
	(void)resources;

	CLAY({ .id = CLAY_ID("GCrtOptionsContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { kPanelPad, kPanelPad, kPanelPad, kPanelPad },
	           .childGap = kFormRowGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtOptionsTitleWrap") }) {
			BankText(CLAY_STRING("Game Options"),
			         BankTextVariant::Heading, {});
		}

		struct LabelRow { const char * label; const char * id; };
		constexpr LabelRow kLabels[6] = {
			{ "Security:",    "GCrtRowSec"   },
			{ "Min Level:",   "GCrtRowMinL"  },
			{ "Max Level:",   "GCrtRowMaxL"  },
			{ "Max Players:", "GCrtRowMaxP"  },
			{ "Max Teams:",   "GCrtRowMaxT"  },
			{ "Spectatable:", "GCrtRowSpect" },
		};

		struct NumInput { const char * id; const char * label; const char * actionId; char * buf; int cap; };
		NumInput kTextInputs[4] = {
			{ "GCrtMinLevel",   "Min Level",   kActionMinLevel,   state.minLevel,   (int)sizeof(state.minLevel)   },
			{ "GCrtMaxLevel",   "Max Level",   kActionMaxLevel,   state.maxLevel,   (int)sizeof(state.maxLevel)   },
			{ "GCrtMaxPlayers", "Max Players", kActionMaxPlayers, state.maxPlayers, (int)sizeof(state.maxPlayers) },
			{ "GCrtMaxTeams",   "Max Teams",   kActionMaxTeams,   state.maxTeams,   (int)sizeof(state.maxTeams)   },
		};

		for(int i = 0; i < 6; ++i){
			CLAY({ .id = CLAY_SID(StaticId(kLabels[i].id)),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(kFormRowH) },
			           .childGap = kFormColumnGap,
			           .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				CLAY({ .id = CLAY_SIDI(CLAY_STRING("GCrtRowLabel"), (uint32_t)i),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(kFormRowH) },
				           .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
				       } }) {
					BankText(FromCStr(kLabels[i].label),
					         BankTextVariant::Body, {});
				}
				if(i == 0){
					BankButton(FromCStr(SecurityLabel(state.securityIndex)),
					           BankButtonVariant::Inline, {},
					           BankButtonHandle{ nullptr, kActionSecurity });
					RegisterButton("Security", kActionSecurity,
					               kValueX, kFormTop + 2, 60, kRowHeight);
				}else if(i == 5){
					BankButton(FromCStr(state.spectatable ? "Yes" : "No"),
					           BankButtonVariant::Inline, {},
					           BankButtonHandle{ nullptr, kActionSpectatable });
					RegisterButton("Spectatable", kActionSpectatable,
					               kValueX, kFormTop + 5*kYSpace + 2, 30, kRowHeight,
					               state.spectatable);
				}else{
					NumInput & ti = kTextInputs[i - 1];
					TextInputOpts opts;
					opts.widthPx     = 20;
					opts.heightPx    = kRowHeight;
					opts.fontBank    = 133;
					opts.fontWidth   = 6;
					opts.brightness  = 128;
					opts.numbersOnly = true;
					opts.showCaret   = false;
					std::string idStr = std::string("Input_") + ti.id;
					TextInput(Clay_String{ false, (int32_t)idStr.size(), idStr.c_str() },
					          ti.buf, opts, {});
					const int y = kFormTop + i * kYSpace + 2;
					RegisterTextInput(ti.label, ti.actionId,
					                  kValueX, y, 20, kRowHeight, ti.buf, ti.cap);
				}
			}
		}
	}
}

void BuildGameCreateTallTree(GameCreatePanelState & state,
                             Resources & resources) {
	(void)resources;

	CLAY({ .id = CLAY_ID("GCrtTallContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { kPanelPad, kPanelPad, kPanelPad, kPanelPad },
	           .childGap = kTallSectionGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtSelectMapTitleWrap") }) {
			BankText(CLAY_STRING("Select Map"),
			         BankTextVariant::Heading, {});
		}

		const int slotCount = std::min((int)state.maps.size(), kMaxMapRows);
		for(int i = 0; i < slotCount; ++i){
			const std::string & raw = state.maps[i];
			const char * txt = raw.c_str();
			size_t len = raw.size();
			if(len >= 5 && std::memcmp(txt, "[DL] ", 5) == 0){ txt += 5; len -= 5; }
			g_mapSlab[i] = Clay_String{ false, (int32_t)len, txt };
		}
		ScrollListOpts listOpts;
		listOpts.width          = kMapListW;
		listOpts.height         = kMapListH;
		listOpts.lineHeight     = kMapListLineH;
		listOpts.highlightColor = 180;
		listOpts.textVariant    = BankTextVariant::Body;
		listOpts.scrollbarBank  = kScrollbarBank;
		CLAY({ .id = CLAY_ID("GCrtMapListWrap") }) {
			ScrollList(CLAY_STRING("GCrtMapList"),
			           g_mapSlab, slotCount,
			           state.mapSelectedIndex, state.mapScrollPos,
			           listOpts,
			           ScrollListHandle{ nullptr, kActionMapPrefix });
		}
		for(int i = 0; i < slotCount; ++i){
			RegisterListRow(g_mapSlab[i].chars,
			                std::string(kActionMapPrefix) + "." + std::to_string(i),
			                kMapListX, kMapListY + i * kMapListLineH,
			                kMapListW, kMapListLineH,
			                i,
			                state.mapSelectedIndex == i);
		}

		CLAY({ .id = CLAY_ID("GCrtNameLabelWrap") }) {
			BankText(CLAY_STRING("Game name:"),
			         BankTextVariant::Heading, {});
		}
		TextInputOpts bodyInput;
		bodyInput.widthPx    = kNameInputW;
		bodyInput.heightPx   = kNameInputH;
		bodyInput.fontBank   = 133;
		bodyInput.fontWidth  = 6;
		bodyInput.brightness = 128;
		bodyInput.showCaret  = false;
		CLAY({ .id = CLAY_ID("GCrtNameInputWrap") }) {
			TextInput(CLAY_STRING("GCrtNameInput"),
			          state.name, bodyInput, {});
		}
		RegisterTextInput("Game name", kActionName,
		                  kNameInputX, kNameInputY, kNameInputW, kNameInputH,
		                  state.name, (int)sizeof(state.name));

		CLAY({ .id = CLAY_ID("GCrtPwLabelWrap") }) {
			BankText(CLAY_STRING("Password (optional):"),
			         BankTextVariant::Heading, {});
		}
		bodyInput.password = true;
		CLAY({ .id = CLAY_ID("GCrtPwInputWrap") }) {
			TextInput(CLAY_STRING("GCrtPwInput"),
			          state.password, bodyInput, {});
		}
		RegisterTextInput("Password", kActionPassword,
		                  kPwInputX, kPwInputY, kPwInputW, kPwInputH,
		                  state.password, (int)sizeof(state.password), true);

		CLAY({ .id = CLAY_ID("GCrtCreateBtnWrap"),
		       .layout = { .childAlignment = { .x = CLAY_ALIGN_X_CENTER } } }) {
			BankButton(CLAY_STRING("Create"),
			           BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, kActionCreate });
		}
		RegisterButton("Create", kActionCreate, kCreateBtnX, kCreateBtnY, 156, 21);
	}
}

}  // namespace silencer::client_ui::lobby
