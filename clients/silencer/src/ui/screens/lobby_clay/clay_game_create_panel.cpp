#include "clay_game_create_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "clay_inspector.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/scroll_list.h"
#include "primitives/text_input.h"

#include "lobby_clay_screen.h"
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

namespace silencer::ui::lobby_clay {

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

// LobbyRightUpperBox interior layout knobs. Box at (238, 64, 160x121).
constexpr uint16_t kUpperHeadingPadLeft = 7;  // 245 - 238
constexpr uint16_t kUpperHeadingPadTop  = 6;  // 70 - 64
constexpr uint16_t kUpperFormPadLeft    = 5;  // 243 - 238
constexpr uint16_t kUpperFormPadTop     = 2;  // 87 - (64 + 6 + 15)
constexpr uint16_t kUpperFormRowH       = 14;
constexpr uint16_t kUpperFormLabelPadLeft = 4;  // 247 - 243
constexpr uint16_t kUpperFormValueGap    = 72;  // label takes ~12 chars × 6 = 72 → value starts near x=323

// LobbyRightTallBox interior layout knobs. Box at (398, 64, 232x391).
constexpr uint16_t kTallHeadingPadLeft = 7;   // 405 - 398
constexpr uint16_t kTallHeadingPadTop  = 6;   // 70 - 64
constexpr uint16_t kTallMapListPadLeft = 9;   // 407 - 398
constexpr uint16_t kTallMapListPadTop  = 4;   // 89 - (64 + 6 + 15)
constexpr uint16_t kTallNameLabelPadLeft = 7; // 405 - 398
constexpr uint16_t kTallNameLabelPadTop  = 6; // 360 - (89 + 265)
constexpr uint16_t kTallInputPadLeft     = 12;// 410 - 398
constexpr uint16_t kTallInputPadTop      = 0; // 375 - (360 + 15)
constexpr uint16_t kTallPwLabelPadTop    = 1; // 390 - (375 + 14)
constexpr uint16_t kTallPwInputPadTop    = 0; // 405 - (390 + 15)
constexpr uint16_t kTallCreatePadLeft    = 38;// 436 - 398
constexpr uint16_t kTallCreatePadTop     = 11;// 430 - (405 + 14)

constexpr int kMaxMapRows = 1024;
Clay_String g_mapSlab[kMaxMapRows];

Clay_String FromCStr(const char * s) {
	return Clay_String{ false, static_cast<int32_t>(strlen(s)), s };
}

Clay_String StaticId(const char * s) {
	return Clay_String{ true, static_cast<int32_t>(strlen(s)), s };
}

void OnSecurityClicked(void * user)    { static_cast<GameCreatePanelState *>(user)->securityClicked    = true; }
void OnSpectatableClicked(void * user) { static_cast<GameCreatePanelState *>(user)->spectatableClicked = true; }
void OnCreateClicked(void * user)      { static_cast<GameCreatePanelState *>(user)->createClicked      = true; }
void OnMapRowSelected(void * user, int index) {
	static_cast<GameCreatePanelState *>(user)->mapRowClickedIndex = index;
}

const char * SecurityLabel(Uint8 idx) {
	switch(idx){
		case 0:  return "Off";
		case 1:  return "Low";
		case 3:  return "High";
		default: return "Medium";
	}
}

void RegisterButton(const char * label, int x, int y, int w, int h,
                    void (*onClick)(void *), void * user, bool selected = false) {
	silencer::ui::clay_inspector::Widget reg;
	reg.label     = label;
	reg.kind      = silencer::ui::clay_inspector::WidgetKind::Button;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	reg.onClick   = onClick;
	reg.clickUser = user;
	reg.selected  = selected;
	silencer::ui::clay_inspector::Register(reg);
}

void RegisterTextInput(const char * label, int x, int y, int w, int h,
                       char * buf, int cap, bool isPassword = false) {
	silencer::ui::clay_inspector::Widget reg;
	reg.label         = label;
	reg.kind          = silencer::ui::clay_inspector::WidgetKind::TextInput;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	reg.textBuffer    = buf;
	reg.textBufferLen = cap;
	reg.isPassword    = isPassword;
	silencer::ui::clay_inspector::Register(reg);
}

void RegisterListRow(const char * label, int x, int y, int w, int h,
                     void (*onClickRow)(void *, int), void * user,
                     int rowIndex, bool selected) {
	silencer::ui::clay_inspector::Widget reg;
	reg.label      = label;
	reg.kind       = silencer::ui::clay_inspector::WidgetKind::ListRow;
	reg.x = x; reg.y = y; reg.w = w; reg.h = h;
	reg.onClickRow = onClickRow;
	reg.clickUser  = user;
	reg.rowIndex   = rowIndex;
	reg.selected   = selected;
	silencer::ui::clay_inspector::Register(reg);
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
                         LobbyClayScreen & owner) {
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

void BuildGameCreateUpperTree(GameCreatePanelState & state,
                              Resources & resources) {
	(void)resources;

	// "Game Options" heading at top.
	CLAY({ .id = CLAY_ID("GCrtOptionsTitleWrap"),
	       .layout = { .padding = { kUpperHeadingPadLeft, 0,
	                                kUpperHeadingPadTop,  0 } } }) {
		BankText(CLAY_STRING("Game Options"),
		         BankTextVariant::Heading, {});
	}

	// 6-row form: each row is LEFT_TO_RIGHT (label + value), fixed height.
	struct LabelRow { const char * label; const char * id; };
	constexpr LabelRow kLabels[6] = {
		{ "Security:",    "GCrtRowSec"   },
		{ "Min Level:",   "GCrtRowMinL"  },
		{ "Max Level:",   "GCrtRowMaxL"  },
		{ "Max Players:", "GCrtRowMaxP"  },
		{ "Max Teams:",   "GCrtRowMaxT"  },
		{ "Spectatable:", "GCrtRowSpect" },
	};

	struct NumInput { const char * id; const char * label; char * buf; int cap; };
	NumInput kTextInputs[4] = {
		{ "GCrtMinLevel",   "Min Level",   state.minLevel,   (int)sizeof(state.minLevel)   },
		{ "GCrtMaxLevel",   "Max Level",   state.maxLevel,   (int)sizeof(state.maxLevel)   },
		{ "GCrtMaxPlayers", "Max Players", state.maxPlayers, (int)sizeof(state.maxPlayers) },
		{ "GCrtMaxTeams",   "Max Teams",   state.maxTeams,   (int)sizeof(state.maxTeams)   },
	};

	// First row needs padTop from heading bottom; rest stack contiguous.
	for(int i = 0; i < 6; ++i){
		const uint16_t rowPadTop = (i == 0) ? kUpperFormPadTop : 0;
		CLAY({ .id = CLAY_SID(StaticId(kLabels[i].id)),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(kUpperFormRowH) },
		           .padding = { kUpperFormPadLeft, 0, rowPadTop, 0 },
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			// Label column.
			CLAY({ .id = CLAY_SID(StaticId((std::string("Lbl_") + kLabels[i].id).c_str())),
			       .layout = { .padding = { kUpperFormLabelPadLeft, 0, 0, 0 } } }) {
				BankText(FromCStr(kLabels[i].label),
				         BankTextVariant::Body, {});
			}
			// Spacer to push value rightward.
			CLAY({ .id = CLAY_SID(StaticId((std::string("Spc_") + kLabels[i].id).c_str())),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(kUpperFormValueGap),
			                       CLAY_SIZING_FIXED(0) },
			       } }) {}
			// Value column.
			if(i == 0){
				BankButton(FromCStr(SecurityLabel(state.securityIndex)),
				           BankButtonVariant::Inline, {},
				           BankButtonHandle{ nullptr, &OnSecurityClicked, &state });
				RegisterButton("Security", kValueX, kFormTop + 2, 60, kRowHeight,
				               &OnSecurityClicked, &state);
			}else if(i == 5){
				BankButton(FromCStr(state.spectatable ? "Yes" : "No"),
				           BankButtonVariant::Inline, {},
				           BankButtonHandle{ nullptr, &OnSpectatableClicked, &state });
				RegisterButton("Spectatable", kValueX, kFormTop + 5*kYSpace + 2, 30, kRowHeight,
				               &OnSpectatableClicked, &state, state.spectatable);
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
				RegisterTextInput(ti.label, kValueX, y, 20, kRowHeight, ti.buf, ti.cap);
			}
		}
	}
}

void BuildGameCreateTallTree(GameCreatePanelState & state,
                             Resources & resources) {
	(void)resources;

	// "Select Map" heading at top.
	CLAY({ .id = CLAY_ID("GCrtSelectMapTitleWrap"),
	       .layout = { .padding = { kTallHeadingPadLeft, 0,
	                                kTallHeadingPadTop,  0 } } }) {
		BankText(CLAY_STRING("Select Map"),
		         BankTextVariant::Heading, {});
	}

	// Map ScrollList.
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
	CLAY({ .id = CLAY_ID("GCrtMapListWrap"),
	       .layout = { .padding = { kTallMapListPadLeft, 0,
	                                kTallMapListPadTop,  0 } } }) {
		ScrollList(CLAY_STRING("GCrtMapList"),
		           g_mapSlab, slotCount,
		           state.mapSelectedIndex, state.mapScrollPos,
		           listOpts,
		           ScrollListHandle{ nullptr, &OnMapRowSelected, &state });
	}
	for(int i = 0; i < slotCount; ++i){
		RegisterListRow(g_mapSlab[i].chars,
		                kMapListX, kMapListY + i * kMapListLineH,
		                kMapListW, kMapListLineH,
		                &OnMapRowSelected, &state, i,
		                state.mapSelectedIndex == i);
	}

	// "Game name:" label.
	CLAY({ .id = CLAY_ID("GCrtNameLabelWrap"),
	       .layout = { .padding = { kTallNameLabelPadLeft, 0,
	                                kTallNameLabelPadTop,  0 } } }) {
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
	CLAY({ .id = CLAY_ID("GCrtNameInputWrap"),
	       .layout = { .padding = { kTallInputPadLeft, 0,
	                                kTallInputPadTop,  0 } } }) {
		TextInput(CLAY_STRING("GCrtNameInput"),
		          state.name, bodyInput, {});
	}
	RegisterTextInput("Game name", kNameInputX, kNameInputY, kNameInputW, kNameInputH,
	                  state.name, (int)sizeof(state.name));

	// "Password (optional):" label.
	CLAY({ .id = CLAY_ID("GCrtPwLabelWrap"),
	       .layout = { .padding = { kTallNameLabelPadLeft, 0,
	                                kTallPwLabelPadTop, 0 } } }) {
		BankText(CLAY_STRING("Password (optional):"),
		         BankTextVariant::Heading, {});
	}
	bodyInput.password = true;
	CLAY({ .id = CLAY_ID("GCrtPwInputWrap"),
	       .layout = { .padding = { kTallInputPadLeft, 0,
	                                kTallPwInputPadTop, 0 } } }) {
		TextInput(CLAY_STRING("GCrtPwInput"),
		          state.password, bodyInput, {});
	}
	RegisterTextInput("Password", kPwInputX, kPwInputY, kPwInputW, kPwInputH,
	                  state.password, (int)sizeof(state.password), true);

	// Create button.
	CLAY({ .id = CLAY_ID("GCrtCreateBtnWrap"),
	       .layout = { .padding = { kTallCreatePadLeft, 0,
	                                kTallCreatePadTop, 0 } } }) {
		BankButton(CLAY_STRING("Create"),
		           BankButtonVariant::Chrome, {},
		           BankButtonHandle{ nullptr, &OnCreateClicked, &state });
	}
	RegisterButton("Create", kCreateBtnX, kCreateBtnY, 156, 21,
	               &OnCreateClicked, &state);
}

}  // namespace silencer::ui::lobby_clay
