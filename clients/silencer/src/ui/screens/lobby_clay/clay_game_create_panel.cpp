#include "clay_game_create_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "clay_inspector.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/form_border.h"
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
using silencer::ui::primitives::FormBorder;
using silencer::ui::primitives::ScrollList;
using silencer::ui::primitives::ScrollListHandle;
using silencer::ui::primitives::ScrollListOpts;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;

namespace silencer::ui::lobby_clay {

namespace {

// Legacy GameCreatePanel coordinates — pixel-identical to the legacy panel.
constexpr int    kYOffset      = 2;
constexpr int    kYSpace       = 14;
constexpr int    kRowHeight    = 14;
constexpr int    kLabelX       = 247;
constexpr int    kValueX       = 323;
constexpr int    kFormLeft     = 243;
constexpr int    kFormTop      = 87;
constexpr int    kFormWidth    = 156;
constexpr int    kFormHeight   = 93;
// Palette index of the lobby chrome's bright-green stroke (sampled from
// the baked BG sprite (7, 1)). Lives at panel scope until the chrome-via-
// primitives milestone introduces a shared lobby theme.
constexpr Uint8  kFormStroke   = 220;

constexpr int    kMapListX     = 407;
constexpr int    kMapListY     = 89;
constexpr Uint16 kMapListW     = 214;
constexpr Uint16 kMapListH     = 265;
constexpr Uint8  kMapListLineH = 14;
// Sprite bank carrying the lobby chrome's scrollbar art (track idx 9, thumb
// idx 10). Lives at panel scope until the chrome-via-primitives milestone
// introduces a shared lobby theme.
constexpr Uint8  kScrollbarBank = 7;

constexpr int    kNameInputX = 410, kNameInputY = 375;
constexpr Uint16 kNameInputW = 210, kNameInputH = 14;
constexpr int    kPwInputX   = 410, kPwInputY   = 405;
constexpr Uint16 kPwInputW   = 210, kPwInputH   = 14;

constexpr int    kCreateBtnX = 436, kCreateBtnY = 430;

// Per-frame slab for the map list display strings.
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
		default: return "Medium";  // 2 + fallback
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
	// Remote-only maps get a "[DL] " prefix key so the Create flow can detect
	// them via servermaps lookup; the row renderer strips the prefix for display.
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

	// Deferred CreateGame state machine — mirrors LobbyScreen::Tick's
	// `if(gameCreate)` block so the upload+CreateGame handshake completes
	// even while the user is waiting on the progress modal.
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

	// Create-button kickoff — mirrors GameCreatePanel::Tick's GCRT_BTN_CREATE case.
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

void BuildGameCreatePanelTree(GameCreatePanelState & state,
                              Resources & resources) {
	// Right border chrome sprite (bank 7 idx 8) — float at -spriteoffset to
	// match the legacy renderer's Overlay positioning.
	const Uint16 borderW = resources.spritewidth[7][8];
	const Uint16 borderH = resources.spriteheight[7][8];
	const int borderX = -resources.spriteoffsetx[7][8];
	const int borderY = -resources.spriteoffsety[7][8];
	CLAY({ .id = CLAY_ID("GCrtRightBorder"),
	       .layout = { .sizing = { CLAY_SIZING_FIXED((float)borderW), CLAY_SIZING_FIXED((float)borderH) } },
	       .image    = { .imageData = silencer::clay_bridge::PackImage(7, 8) },
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { (float)borderX, (float)borderY } } }) {}

	// "Game Options" header (left of form).
	CLAY({ .id = CLAY_ID("GCrtOptionsTitleWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { kFormLeft + 2, 70 } } }) {
		BankText(CLAY_STRING("Game Options"), BankTextVariant::Heading, {});
	}

	// 1-px form border.
	CLAY({ .id = CLAY_ID("GCrtFormBorder"),
	       .layout = { .sizing = { CLAY_SIZING_FIXED(kFormWidth), CLAY_SIZING_FIXED(kFormHeight) } },
	       .border   = FormBorder(kFormStroke),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { kFormLeft, kFormTop } } }) {}

	// Six row labels at labelX, y = form_top + i*yspace + yoffset.
	struct LabelRow { const char * label; const char * id; };
	constexpr LabelRow kLabels[6] = {
		{ "Security:",    "GCrtLblSec"   },
		{ "Min Level:",   "GCrtLblMinL"  },
		{ "Max Level:",   "GCrtLblMaxL"  },
		{ "Max Players:", "GCrtLblMaxP"  },
		{ "Max Teams:",   "GCrtLblMaxT"  },
		{ "Spectatable:", "GCrtLblSpect" },
	};
	for(int i = 0; i < 6; ++i){
		const float y = (float)(kFormTop + kYSpace * i + kYOffset);
		CLAY({ .id = CLAY_SID(StaticId(kLabels[i].id)),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { kLabelX, y } } }) {
			BankText(FromCStr(kLabels[i].label), BankTextVariant::Body, {});
		}
	}

	// Row 0: Security cycler.
	const float secY = (float)(kFormTop + kYOffset);
	CLAY({ .id = CLAY_ID("GCrtSecValWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { kValueX, secY } } }) {
		BankButton(FromCStr(SecurityLabel(state.securityIndex)),
		           BankButtonVariant::Inline, {},
		           BankButtonHandle{ nullptr, &OnSecurityClicked, &state });
	}
	RegisterButton("Security", kValueX, (int)secY, 60, kRowHeight, &OnSecurityClicked, &state);

	// Rows 1-4: Min/Max Level, Max Players, Max Teams (TextInput, numbersOnly).
	struct NumInput { const char * id; const char * label; int row; char * buf; int cap; };
	const NumInput kTextInputs[4] = {
		{ "GCrtMinLevel",   "Min Level",   1, state.minLevel,   (int)sizeof(state.minLevel)   },
		{ "GCrtMaxLevel",   "Max Level",   2, state.maxLevel,   (int)sizeof(state.maxLevel)   },
		{ "GCrtMaxPlayers", "Max Players", 3, state.maxPlayers, (int)sizeof(state.maxPlayers) },
		{ "GCrtMaxTeams",   "Max Teams",   4, state.maxTeams,   (int)sizeof(state.maxTeams)   },
	};
	for(const auto & ti : kTextInputs){
		const int y = kFormTop + kYSpace * ti.row + kYOffset;
		CLAY({ .id = CLAY_SID(StaticId(ti.id)),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { kValueX, (float)y } } }) {
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
		}
		RegisterTextInput(ti.label, kValueX, y, 20, kRowHeight, ti.buf, ti.cap);
	}

	// Row 5: Spectatable cycler.
	const int spectY = kFormTop + kYSpace * 5 + kYOffset;
	CLAY({ .id = CLAY_ID("GCrtSpectValWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { kValueX, (float)spectY } } }) {
		BankButton(FromCStr(state.spectatable ? "Yes" : "No"),
		           BankButtonVariant::Inline, {},
		           BankButtonHandle{ nullptr, &OnSpectatableClicked, &state });
	}
	RegisterButton("Spectatable", kValueX, spectY, 30, kRowHeight,
	               &OnSpectatableClicked, &state, /*selected*/ state.spectatable);

	// "Select Map" header + map ScrollList.
	CLAY({ .id = CLAY_ID("GCrtSelectMapTitleWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { 405, 70 } } }) {
		BankText(CLAY_STRING("Select Map"), BankTextVariant::Heading, {});
	}

	const int slotCount = std::min((int)state.maps.size(), kMaxMapRows);
	for(int i = 0; i < slotCount; ++i){
		// Strip the "[DL] " sentinel for display; legacy paints a "DL" badge in
		// the right margin which we omit (~16x11 expected diff per remote row).
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
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { kMapListX, kMapListY } } }) {
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
		                /*selected*/ state.mapSelectedIndex == i);
	}

	// Game name + password labels and inputs (Heading label, 210x14 Body input).
	CLAY({ .id = CLAY_ID("GCrtNameLabelWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { 405, 360 } } }) {
		BankText(CLAY_STRING("Game name:"), BankTextVariant::Heading, {});
	}
	CLAY({ .id = CLAY_ID("GCrtPwLabelWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { 405, 390 } } }) {
		BankText(CLAY_STRING("Password (optional):"), BankTextVariant::Heading, {});
	}
	TextInputOpts bodyInput;
	bodyInput.widthPx    = kNameInputW;
	bodyInput.heightPx   = kNameInputH;
	bodyInput.fontBank   = 133;
	bodyInput.fontWidth  = 6;
	bodyInput.brightness = 128;
	bodyInput.showCaret  = false;
	CLAY({ .id = CLAY_ID("GCrtNameInputWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { kNameInputX, kNameInputY } } }) {
		TextInput(CLAY_STRING("GCrtNameInput"), state.name, bodyInput, {});
	}
	RegisterTextInput("Game name", kNameInputX, kNameInputY, kNameInputW, kNameInputH,
	                  state.name, (int)sizeof(state.name));
	bodyInput.password = true;
	CLAY({ .id = CLAY_ID("GCrtPwInputWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { kPwInputX, kPwInputY } } }) {
		TextInput(CLAY_STRING("GCrtPwInput"), state.password, bodyInput, {});
	}
	RegisterTextInput("Password", kPwInputX, kPwInputY, kPwInputW, kPwInputH,
	                  state.password, (int)sizeof(state.password), /*isPassword*/ true);

	// Create button (Chrome variant).
	const int createOffX = kCreateBtnX - resources.spriteoffsetx[7][24];
	const int createOffY = kCreateBtnY - resources.spriteoffsety[7][24];
	CLAY({ .id = CLAY_ID("GCrtCreateBtnWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, .offset = { (float)createOffX, (float)createOffY } } }) {
		BankButton(CLAY_STRING("Create"), BankButtonVariant::Chrome, {},
		           BankButtonHandle{ nullptr, &OnCreateClicked, &state });
	}
	RegisterButton("Create", kCreateBtnX, kCreateBtnY, 156, 21, &OnCreateClicked, &state);
}

}  // namespace silencer::ui::lobby_clay
