#include "clay_game_create_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
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
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextOpts;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonOpts;
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

// Legacy GameCreatePanel coordinates — copied verbatim from
// game_create_panel.cpp so the on-screen geometry matches one-for-one.
constexpr int    kYOffset      = 2;
constexpr int    kYSpace       = 14;
constexpr int    kRowHeight    = 14;
constexpr int    kLabelX       = 247;
constexpr int    kValueX       = 323;
constexpr int    kFormLeft     = 243;
constexpr int    kFormTop      = 87;
constexpr int    kFormWidth    = 156;
constexpr int    kFormHeight   = 93;

constexpr int    kOptionsTitleX = kFormLeft + 2;
constexpr int    kOptionsTitleY = 70;

// Map list block (right column of the right pane).
constexpr int    kSelectMapTitleX = 405;
constexpr int    kSelectMapTitleY = 70;
constexpr int    kMapListX        = 407;
constexpr int    kMapListY        = 89;
constexpr Uint16 kMapListW        = 214;
constexpr Uint16 kMapListH        = 265;
constexpr Uint8  kMapListLineH    = 14;

// Game name + password block (bottom of the right pane).
constexpr int    kNameLabelX = 405;
constexpr int    kNameLabelY = 360;
constexpr int    kNameInputX = 410;
constexpr int    kNameInputY = 375;
constexpr Uint16 kNameInputW = 210;
constexpr Uint16 kNameInputH = 14;
constexpr int    kPwLabelX = 405;
constexpr int    kPwLabelY = 390;
constexpr int    kPwInputX = 410;
constexpr int    kPwInputY = 405;
constexpr Uint16 kPwInputW = 210;
constexpr Uint16 kPwInputH = 14;

constexpr int    kCreateBtnX = 436;
constexpr int    kCreateBtnY = 430;

// Per-frame slabs / file-scope click adapters. Same conventions as the
// sibling Clay panels: small fixed-size arrays overwritten in place each
// frame.
constexpr int kMaxMapRows = 1024;
Clay_String g_mapSlab[kMaxMapRows];

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

Clay_String FromCStr(const char * s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(strlen(s));
	cs.chars  = s;
	return cs;
}

void OnSecurityClicked(void * user) {
	auto * state = static_cast<GameCreatePanelState *>(user);
	if(state) state->securityClicked = true;
}
void OnSpectatableClicked(void * user) {
	auto * state = static_cast<GameCreatePanelState *>(user);
	if(state) state->spectatableClicked = true;
}
void OnCreateClicked(void * user) {
	auto * state = static_cast<GameCreatePanelState *>(user);
	if(state) state->createClicked = true;
}
void OnMapRowSelected(void * user, int index) {
	auto * state = static_cast<GameCreatePanelState *>(user);
	if(state) state->mapRowClickedIndex = index;
}

const char * SecurityLabel(Uint8 idx) {
	switch(idx){
		case 0:  return "Off";
		case 1:  return "Low";
		case 2:  return "Medium";
		case 3:  return "High";
		default: return "Medium";
	}
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
		if(std::find(maps.begin(), maps.end(), f) == maps.end()){
			maps.push_back(f);
		}
	}
	std::sort(maps.begin(), maps.end());
	for(auto & m : maps){
		state.maps.push_back(m);
	}
	mapDownloader.servermaps.clear();
	auto serverlist = FetchServerMapList(Config::GetInstance().mapapiurl);
	for(auto & entry : serverlist){
		bool alreadylocal = std::find(maps.begin(), maps.end(), entry.first) != maps.end();
		if(!alreadylocal){
			// Match the legacy SelectBox display: stash the "[DL] " sentinel
			// in mapDownloader.servermaps under the same key the legacy uses
			// (lookups in the Create flow depend on it) but render only the
			// bare map name in the list. The legacy renderer specifically
			// strips the "[DL] " prefix and paints a right-margin "DL" badge
			// — for Clay we render the bare name and accept the missing badge
			// pixels (one ~16x11 strip per remote row). The state.maps slot
			// keeps the legacy "[DL] " key so the create-button flow can
			// detect remote-only rows via servermaps lookup.
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
	std::strncpy(state.name,
	             Config::GetInstance().defaultgamename,
	             sizeof(state.name) - 1);
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

	// Apply any row click from the previous frame.
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
	// `if(gameCreate){ ... }` block. Runs continuously while the Clay
	// GameCreate surface is active so the upload+CreateGame handshake can
	// complete even after the user has visually left the form to wait on
	// the progress modal.
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
			// Mirrors LobbyScreen::Tick's host-side gameinfo seeding.
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

	// Create-button kickoff — mirrors GameCreatePanel::Tick's GCRT_BTN_CREATE
	// switch case.
	if(state.createClicked){
		state.createClicked = false;
		if(game.creategameclicked) return;

		const char * gamename = state.name;
		const char * password = (strlen(state.password) > 0) ? state.password : nullptr;
		Uint8 securitylevel = LobbyGame::SECNONE;
		switch(state.securityIndex){
			case 1: securitylevel = LobbyGame::SECLOW; break;
			case 2: securitylevel = LobbyGame::SECMEDIUM; break;
			case 3: securitylevel = LobbyGame::SECHIGH; break;
		}
		Uint8 minlevel   = static_cast<Uint8>(atoi(state.minLevel));
		Uint8 maxlevel   = static_cast<Uint8>(atoi(state.maxLevel));
		Uint8 maxplayers = static_cast<Uint8>(atoi(state.maxPlayers));
		if(maxplayers <= 0) maxplayers = 1;
		Uint8 maxteams = static_cast<Uint8>(atoi(state.maxTeams));
		if(maxteams <= 0) maxteams = 1;
		bool spectatable = state.spectatable;

		if(strlen(gamename) == 0){
			ctx.ShowMessage("No game name");
			return;
		}
		if(state.mapSelectedIndex < 0 || state.mapSelectedIndex >= (int)state.maps.size()){
			ctx.ShowMessage("No map selected");
			return;
		}
		std::string mapname = state.maps[state.mapSelectedIndex];
		if(mapDownloader.servermaps.count(mapname) > 0){
			ctx.ShowMessage("Download the map first");
			return;
		}
		unsigned char maphash[20];
		mapDownloader.CalculateMapHash(mapDownloader.FindMap(mapname.c_str()).c_str(), &maphash);
		mapDownloader.pendingCreate.gamename = gamename;
		mapDownloader.pendingCreate.mapname  = mapname;
		mapDownloader.pendingCreate.password = password ? password : "";
		memcpy(mapDownloader.pendingCreate.maphash, maphash, 20);
		mapDownloader.pendingCreate.securitylevel = securitylevel;
		mapDownloader.pendingCreate.minlevel      = minlevel;
		mapDownloader.pendingCreate.maxlevel      = maxlevel;
		mapDownloader.pendingCreate.maxplayers    = maxplayers;
		mapDownloader.pendingCreate.maxteams      = maxteams;
		mapDownloader.pendingCreate.spectatable   = spectatable;
		if(mapDownloader.mapUploadThread.joinable()) mapDownloader.mapUploadThread.detach();
		uint32_t gen = ++mapDownloader.mapUploadGeneration;
		std::string mppath = mapDownloader.FindMap(mapname.c_str());
		std::string dataDir = GetDataDir();
		bool isBundledMap = dataDir.empty() || mppath.substr(0, dataDir.size()) != dataDir;
		std::string apiURL = Config::GetInstance().mapapiurl;
		if(isBundledMap){
			mapDownloader.mapUploadState.store(2, std::memory_order_release);
		}else{
			mapDownloader.mapUploadState.store(1, std::memory_order_relaxed);
			std::string mapname_str(mapname);
			std::atomic<int> * uploadStatePtr = &mapDownloader.mapUploadState;
			std::atomic<uint32_t> * uploadGenPtr = &mapDownloader.mapUploadGeneration;
			mapDownloader.mapUploadThread = std::thread([mapname_str, mppath, apiURL, gen, uploadStatePtr, uploadGenPtr](){
				bool ok = UploadMapToServer(mapname_str.c_str(), mppath.c_str(), apiURL.c_str());
				if(uploadGenPtr->load(std::memory_order_relaxed) != gen) return;
				uploadStatePtr->store(ok ? 2 : 3, std::memory_order_release);
			});
		}
		world.lobby.creategamestatus = 0;
		game.creategameclicked = true;
		std::strncpy(Config::GetInstance().defaultgamename, gamename, sizeof(Config::GetInstance().defaultgamename) - 1);
		Config::GetInstance().defaultgamename[sizeof(Config::GetInstance().defaultgamename) - 1] = '\0';
		Config::GetInstance().Save();
		ctx.PushScreen(MessageModal::Progress("Uploading map..."));
	}
}

void BuildGameCreatePanelTree(GameCreatePanelState & state,
                              Resources & resources) {
	// Right border chrome sprite (bank 7 idx 8) — legacy positions an
	// Overlay at (0, 0) with the renderer subtracting spriteoffsetx/y
	// before BlitSurface. Mirror by floating at (-spriteoffsetx, -spriteoffsety).
	const Uint16 borderW = resources.spritewidth[7][8];
	const Uint16 borderH = resources.spriteheight[7][8];
	const int borderX = 0 - resources.spriteoffsetx[7][8];
	const int borderY = 0 - resources.spriteoffsety[7][8];
	CLAY({ .id = CLAY_ID("GCrtRightBorder"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(borderW)),
	                       CLAY_SIZING_FIXED(static_cast<float>(borderH)) },
	       },
	       .image    = { .imageData = silencer::clay_bridge::PackImage(7, 8) },
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { static_cast<float>(borderX),
	                                   static_cast<float>(borderY) } } }) {}

	// "Game Options" header (left of form).
	CLAY({ .id = CLAY_ID("GCrtOptionsTitleWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kOptionsTitleX, kOptionsTitleY } } }) {
		BankText(CLAY_STRING("Game Options"),
		         BankTextVariant::Heading,
		         {});
	}

	// 1-px form border at (form_left, form_top) sized (form_width, form_height).
	CLAY({ .id = CLAY_ID("GCrtFormBorder"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kFormWidth),
	                       CLAY_SIZING_FIXED(kFormHeight) },
	       },
	       .border   = FormBorder(),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kFormLeft, kFormTop } } }) {}

	// Six rows: Security, Min Level, Max Level, Max Players, Max Teams,
	// Spectatable. Each row has a Body label at labelX and a value widget
	// at valueX, both at y = form_top + i*yspace + yoffset.
	struct LabelRow { int y; const char * label; const char * id; };
	const LabelRow kLabels[6] = {
		{ kFormTop + (kYSpace * 0) + kYOffset, "Security:",    "GCrtLblSec"    },
		{ kFormTop + (kYSpace * 1) + kYOffset, "Min Level:",   "GCrtLblMinL"   },
		{ kFormTop + (kYSpace * 2) + kYOffset, "Max Level:",   "GCrtLblMaxL"   },
		{ kFormTop + (kYSpace * 3) + kYOffset, "Max Players:", "GCrtLblMaxP"   },
		{ kFormTop + (kYSpace * 4) + kYOffset, "Max Teams:",   "GCrtLblMaxT"   },
		{ kFormTop + (kYSpace * 5) + kYOffset, "Spectatable:", "GCrtLblSpect"  },
	};
	for(int i = 0; i < 6; ++i){
		Clay_String labelId;
		labelId.isStaticallyAllocated = true;
		labelId.length = static_cast<int32_t>(strlen(kLabels[i].id));
		labelId.chars  = kLabels[i].id;
		CLAY({ .id = CLAY_SID(labelId),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { kLabelX, (float)kLabels[i].y } } }) {
			BankText(FromCStr(kLabels[i].label),
			         BankTextVariant::Body,
			         {});
		}
	}

	// Row values.
	// Security cycler (BankButton::Inline).
	CLAY({ .id = CLAY_ID("GCrtSecValWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kValueX,
	                                   (float)(kFormTop + kYOffset) } } }) {
		BankButton(FromCStr(SecurityLabel(state.securityIndex)),
		           BankButtonVariant::Inline,
		           {},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnSecurityClicked,
		                             /*user*/       &state });
	}

	// Min/Max Level, Max Players, Max Teams (TextInput, numbersOnly).
	const struct { const char * id; int y; char * buf; } kTextInputs[4] = {
		{ "GCrtMinLevel",   kFormTop + (kYSpace * 1) + kYOffset, state.minLevel },
		{ "GCrtMaxLevel",   kFormTop + (kYSpace * 2) + kYOffset, state.maxLevel },
		{ "GCrtMaxPlayers", kFormTop + (kYSpace * 3) + kYOffset, state.maxPlayers },
		{ "GCrtMaxTeams",   kFormTop + (kYSpace * 4) + kYOffset, state.maxTeams },
	};
	for(int i = 0; i < 4; ++i){
		Clay_String wrapId;
		wrapId.isStaticallyAllocated = true;
		wrapId.length = static_cast<int32_t>(strlen(kTextInputs[i].id));
		wrapId.chars  = kTextInputs[i].id;
		CLAY({ .id = CLAY_SID(wrapId),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { kValueX,
		                                   (float)kTextInputs[i].y } } }) {
			TextInputOpts opts;
			opts.widthPx     = 20;
			opts.heightPx    = kRowHeight;
			opts.fontBank    = 133;
			opts.fontWidth   = 6;
			opts.brightness  = 128;
			opts.numbersOnly = true;
			opts.showCaret   = false;
			std::string idStr = std::string("Input_") + kTextInputs[i].id;
			Clay_String inputId;
			inputId.isStaticallyAllocated = false;
			inputId.length = static_cast<int32_t>(idStr.size());
			inputId.chars  = idStr.c_str();
			TextInput(inputId, kTextInputs[i].buf, opts, {});
		}
	}

	// Spectatable cycler (BankButton::Inline).
	CLAY({ .id = CLAY_ID("GCrtSpectValWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kValueX,
	                                   (float)(kFormTop + (kYSpace * 5) + kYOffset) } } }) {
		BankButton(FromCStr(state.spectatable ? "Yes" : "No"),
		           BankButtonVariant::Inline,
		           {},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnSpectatableClicked,
		                             /*user*/       &state });
	}

	// "Select Map" header + map ScrollList.
	CLAY({ .id = CLAY_ID("GCrtSelectMapTitleWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kSelectMapTitleX, kSelectMapTitleY } } }) {
		BankText(CLAY_STRING("Select Map"),
		         BankTextVariant::Heading,
		         {});
	}

	const int rowCount = static_cast<int>(state.maps.size());
	const int slotCount = (rowCount < kMaxMapRows) ? rowCount : kMaxMapRows;
	for(int i = 0; i < slotCount; ++i){
		// Mirror the legacy SelectBox row display: strip the "[DL] " sentinel
		// so the row text shows only the bare map name. The legacy renderer
		// also paints a "DL" badge in the right margin — we omit it here
		// (~16x11 pixels per remote row of expected diff).
		const std::string & raw = state.maps[i];
		const char * txt = raw.c_str();
		size_t len = raw.size();
		if(len >= 5 && std::memcmp(txt, "[DL] ", 5) == 0){
			txt += 5;
			len -= 5;
		}
		Clay_String cs;
		cs.isStaticallyAllocated = false;
		cs.length = static_cast<int32_t>(len);
		cs.chars  = txt;
		g_mapSlab[i] = cs;
	}
	ScrollListOpts listOpts;
	listOpts.width          = kMapListW;
	listOpts.height         = kMapListH;
	listOpts.lineHeight     = kMapListLineH;
	listOpts.highlightColor = 180;
	listOpts.textVariant    = BankTextVariant::Body;
	CLAY({ .id = CLAY_ID("GCrtMapListWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kMapListX, kMapListY } } }) {
		ScrollList(CLAY_STRING("GCrtMapList"),
		           g_mapSlab,
		           slotCount,
		           state.mapSelectedIndex,
		           state.mapScrollPos,
		           listOpts,
		           ScrollListHandle{ /*hoveredOut*/ nullptr,
		                             /*onSelect*/   &OnMapRowSelected,
		                             /*user*/       &state });
	}

	// Game name label + input.
	CLAY({ .id = CLAY_ID("GCrtNameLabelWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kNameLabelX, kNameLabelY } } }) {
		BankText(CLAY_STRING("Game name:"),
		         BankTextVariant::Heading,
		         {});
	}
	CLAY({ .id = CLAY_ID("GCrtNameInputWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kNameInputX, kNameInputY } } }) {
		TextInputOpts opts;
		opts.widthPx     = kNameInputW;
		opts.heightPx    = kNameInputH;
		opts.fontBank    = 133;
		opts.fontWidth   = 6;
		opts.brightness  = 128;
		opts.showCaret   = false;
		TextInput(CLAY_STRING("GCrtNameInput"), state.name, opts, {});
	}

	// Password label + input.
	CLAY({ .id = CLAY_ID("GCrtPwLabelWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kPwLabelX, kPwLabelY } } }) {
		BankText(CLAY_STRING("Password (optional):"),
		         BankTextVariant::Heading,
		         {});
	}
	CLAY({ .id = CLAY_ID("GCrtPwInputWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kPwInputX, kPwInputY } } }) {
		TextInputOpts opts;
		opts.widthPx     = kPwInputW;
		opts.heightPx    = kPwInputH;
		opts.fontBank    = 133;
		opts.fontWidth   = 6;
		opts.password    = true;
		opts.brightness  = 128;
		opts.showCaret   = false;
		TextInput(CLAY_STRING("GCrtPwInput"), state.password, opts, {});
	}

	// Create button (Chrome variant).
	const int createOffX = kCreateBtnX - resources.spriteoffsetx[7][24];
	const int createOffY = kCreateBtnY - resources.spriteoffsety[7][24];
	CLAY({ .id = CLAY_ID("GCrtCreateBtnWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { (float)createOffX, (float)createOffY } } }) {
		BankButton(CLAY_STRING("Create"),
		           BankButtonVariant::Chrome,
		           {},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnCreateClicked,
		                             /*user*/       &state });
	}
}

}  // namespace silencer::ui::lobby_clay
