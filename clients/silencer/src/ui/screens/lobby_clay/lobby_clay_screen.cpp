#include "lobby_clay_screen.h"

#include "screen_context.h"
#include "game.h"
#include "game_state.h"
#include "world.h"
#include "lobby.h"
#include "lobbygame.h"
#include "serializer.h"
#include "config.h"
#include "interface.h"
#include "objecttypes.h"
#include "renderer.h"
#include "resources.h"
#include "surface.h"
#include "ambience_mixer.h"
#include "map_downloader.h"
#include "message_modal.h"
#include "peer.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "clay_inspector.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/scroll_list.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text_input.h"
#include "primitives/toggle.h"

#include <SDL3/SDL.h>

#include <cstring>
#include <memory>
#include <string>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::BankTextBeginFrame;
using silencer::ui::primitives::BankButtonBeginFrame;
using silencer::ui::primitives::ScrollListBeginFrame;
using silencer::ui::primitives::ScrollTextBoxBeginFrame;
using silencer::ui::primitives::TextInputBeginFrame;
using silencer::ui::primitives::ToggleBeginFrame;

LobbyClayScreen::LobbyClayScreen() = default;
LobbyClayScreen::~LobbyClayScreen() = default;

namespace
{
MessageModal * TopAsProgressModal(ScreenContext & ctx)
{
	Screen * top = ctx.game.GetTopScreen();
	if(!top) return nullptr;
	MessageModal * m = dynamic_cast<MessageModal *>(top);
	return (m && m->IsProgress()) ? m : nullptr;
}

bool TopIsModal(ScreenContext & ctx)
{
	Screen * top = ctx.game.GetTopScreen();
	return top && top->IsOverlay();
}

void DismissProgressModal(ScreenContext & ctx)
{
	if(TopAsProgressModal(ctx)) ctx.PopScreen();
}

// Trampoline for the Go Back BankButton's onClick. Defers to the screen's
// per-frame flag so back-routing runs on the next Tick rather than mid-
// layout (calling Game::GoBack mid-Clay would invalidate the screen we're
// currently dispatching commands for).
void OnGoBackClicked(void * user)
{
	auto * screen = static_cast<LobbyClayScreen *>(user);
	if(screen) screen->NotifyGoBackClicked();
}

// Emits the lobby chrome — fullscreen background image (bank 7 idx 1), the
// "Silencer" title, "v.X" version string, optional map-name overlay, and
// the Go Back button. All chrome elements (except the bg image, which
// covers the whole layout) are floating with attachTo=ROOT so their
// positions match the legacy Overlay x/y screen coordinates exactly.
//
// Sprite-element positions COMPENSATE for the resource's per-frame anchor
// offset (`spriteoffsetx/y`): the legacy renderer subtracts those from the
// object's screen-space x/y before blitting, but the Clay bridge blits
// sprite-natively at the bbox top-left. Pre-subtracting the offset in the
// floating .offset keeps the on-screen pixels byte-identical.
void BuildChromeTree(LobbyClayScreen * screen,
                     const std::string & version,
                     const std::string & mapName,
                     Resources & resources)
{
	using namespace silencer::clay_bridge;

	const int W = 640;
	const int H = 480;

	const int bgOffX = -resources.spriteoffsetx[7][1];
	const int bgOffY = -resources.spriteoffsety[7][1];
	const int btnOffX = 473 - resources.spriteoffsetx[7][24];
	const int btnOffY = 29  - resources.spriteoffsety[7][24];

	CLAY({ .id = CLAY_ID("LobbyClayRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	       } }) {
		CLAY({ .id = CLAY_ID("LobbyBg"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
		       },
		       .image = { .imageData = PackImage(7, 1) },
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset = { (float)bgOffX, (float)bgOffY } } }) {}

		CLAY({ .id = CLAY_ID("LobbyTitle"),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset = { 15, 32 } } }) {
			BankText(CLAY_STRING("Silencer"),
			         BankTextVariant::Title,
			         { .effectColor = 152 });
		}

		Clay_String verstr;
		verstr.isStaticallyAllocated = false;
		verstr.length = (int32_t)version.size();
		verstr.chars  = version.c_str();
		CLAY({ .id = CLAY_ID("LobbyVer"),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset = { 115, 39 } } }) {
			BankText(verstr,
			         BankTextVariant::Body,
			         { .effectColor = 189 });
		}

		if(!mapName.empty()){
			Clay_String mstr;
			mstr.isStaticallyAllocated = false;
			mstr.length = (int32_t)mapName.size();
			mstr.chars  = mapName.c_str();
			CLAY({ .id = CLAY_ID("LobbyMapName"),
			       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
			                     .offset = { 180, 32 } } }) {
				BankText(mstr,
				         BankTextVariant::Title,
				         { .effectColor = 129,
				           .brightness  = 160,
				           .colorRamp   = true });
			}
		}

		CLAY({ .id = CLAY_ID("LobbyGoBackWrap"),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset = { (float)btnOffX, (float)btnOffY } } }) {
			BankButton(CLAY_STRING("Go Back"),
			           BankButtonVariant::Chrome,
			           {},
			           { .hoveredOut = nullptr,
			             .onClick = &OnGoBackClicked,
			             .user = screen });
		}
		silencer::ui::clay_inspector::Widget gb;
		gb.label = "Go Back";
		gb.kind = silencer::ui::clay_inspector::WidgetKind::Button;
		gb.x = 473; gb.y = 29; gb.w = 156; gb.h = 21;
		gb.onClick = &OnGoBackClicked;
		gb.clickUser = screen;
		silencer::ui::clay_inspector::Register(gb);
	}
}
}  // namespace

void LobbyClayScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;
	ctx.ResetPresentation(2);
	ctx.renderer.camera.SetPosition(320, 240);

	version  = "v.";
	version += world.GetVersion();
	mapName.clear();
	goBackClicked = false;

	// Parent Interface for child Clay-state inspector registrations. No
	// world Overlay/Button objects for the chrome — the Clay tree paints
	// them in Draw each frame.
	Interface * lobbyiface = static_cast<Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
	interfaceId = lobbyiface->id;

	silencer::ui::lobby_clay::CharacterPanelInit(characterState);
	silencer::ui::lobby_clay::ChatPanelInit(chatState);
	silencer::ui::lobby_clay::GameSelectPanelInit(gameSelectState);
	silencer::ui::lobby_clay::GameJoinPanelInit(gameJoinState);
	gameJoinActive = false;
	silencer::ui::lobby_clay::GameTechPanelInit(gameTechState);
	gameTechActive = false;
	gameCreateActive = false;
}

void LobbyClayScreen::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	Game & game = ctx.game;

	// Lobby disconnect → bounce back to the connect screen.
	if(world.lobby.state == Lobby::DISCONNECTED){
		world.Disconnect();
		ctx.GoToState(GameState::LOBBYCONNECT);
		return;
	}

	// Chrome Go Back — flag was set by the Clay onClick trampoline on the
	// previous frame. Consume it before pumping anything else.
	if(goBackClicked){
		goBackClicked = false;
		if(game.GoBack()) return;
	}

	// Reconcile any agency-toggle click from the previous frame's Clay
	// layout into Config + world. Domain glue (Config::Save, SetAgency)
	// lives here in the screen — the Toggle primitive only reports clicks.
	silencer::ui::lobby_clay::CharacterPanelTick(characterState, ctx.world);

	// Drain lobby chat / presence / channel changes into Clay state.
	silencer::ui::lobby_clay::ChatPanelTick(chatState, ctx.world);

	// Consume the Clay GameSelect panel's per-frame click flags + rebuild
	// rows on lobby updates. Only active when no other right-side panel
	// owns the surface.
	if(!gameCreateActive && !gameJoinActive && !gameTechActive){
		silencer::ui::lobby_clay::GameSelectPanelTick(
			gameSelectState, ctx.world, ctx, *this);
	}

	// Clay GameCreate surface owns its own deferred-create state machine.
	if(gameCreateActive){
		silencer::ui::lobby_clay::GameCreatePanelTick(
			gameCreateState, ctx.world, ctx, *this);
	}

	// Clay GameJoin surface — Ready label rewrite + button click dispatch.
	if(gameJoinActive){
		silencer::ui::lobby_clay::GameJoinPanelTick(
			gameJoinState, ctx.world, ctx, *this);
	}

	// Clay GameTech surface — slots-left refresh, peer-name refresh, checkbox
	// / desc-overlay click dispatch, Back To Teams routing.
	if(gameTechActive){
		silencer::ui::lobby_clay::GameTechPanelTick(
			gameTechState, ctx.world, ctx, *this);
	}

	MapDownloader & mapDownloader = ctx.mapDownloader;

	// Pre-CONNECTED surfaces (gameselect / gamecreate) — join finalisation,
	// progress-modal spinner update, auto-dismiss, CONNECTED→GameJoin
	// transition. Mirrors LobbyScreen::Tick's `if(gameSelect || gameCreate)`
	// block from before the lobby was Clay-driven.
	if(!gameJoinActive && !gameTechActive){
		if(game.joininggame){
			if(world.state == World::CONNECTED){
				game.joininggame = false;
			}
			if(world.state == World::IDLE){
				game.joininggame = false;
				DismissProgressModal(ctx);
				ctx.ShowMessage("Unable to join game");
			}
		}
		if(MessageModal * progress = TopAsProgressModal(ctx)){
			std::string text = (mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 1)
				? "Uploading map" : "Creating game";
			int dots = (world.tickcount / 4) % 6;
			if(dots > 3) dots = 6 - dots;
			for(int i = 0; i < dots; i++) text += ".";
			progress->SetText(ctx, text);
		}
		if(TopAsProgressModal(ctx) && world.lobby.creategamestatus != 100 &&
		   mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 0 &&
		   (world.state == World::CONNECTED || world.state == World::IDLE)){
			ctx.PopScreen();
			game.creategameclicked = false;
		}
		if(world.state == World::CONNECTED && interfaceId){
			Peer * peer = world.peerlist[world.localpeerid];
			if(peer){
				mapDownloader.mapexistchecked = false;
				mapDownloader.mapjoingeneration.fetch_add(1, std::memory_order_relaxed);
				mapDownloader.mapjoinstate.store(0, std::memory_order_relaxed);
				if(mapDownloader.mapjointhread.joinable()) mapDownloader.mapjointhread.detach();
				world.SetTech(Config::GetInstance().defaulttechchoices[Config::GetInstance().defaultagency]);
				ShowGameJoin(ctx);
				LobbyGame * lobbygame = world.lobby.GetGameById(game.currentlobbygameid);
				if(lobbygame){
					char temp[256];
					ctx.ambienceMixer.GetGameChannelName(*lobbygame, temp);
					strcpy(world.lobby.lastchannel, world.lobby.channel);
					world.lobby.JoinChannel(temp);
					SetMapNameOverlay(world, lobbygame->mapname);
				}
			}
		}
	}

	mapDownloader.ProcessMapDownload();

	// Disconnect-from-game modal — fires on the joined-game surface
	// (gameJoinActive || gameTechActive) when the world drops out of
	// CONNECTED. Mirrors the legacy LobbyScreen::Tick `if(gameJoin || gameTech)`
	// trigger; the Clay flags are the equivalent gate.
	if(world.state != World::CONNECTED && !TopIsModal(ctx)){
		if(gameJoinActive || gameTechActive){
			Game * gamePtr = &game;
			ctx.ShowMessage("Disconnected from game", [gamePtr]() { gamePtr->GoBack(); });
		}
	}
}

void LobbyClayScreen::ShowGameSelect(ScreenContext & ctx)
{
	gameCreateActive = false;
	gameJoinActive   = false;
	gameTechActive   = false;
	// Force the games-list to re-snapshot from world.lobby on the next Tick
	// — the legacy ShowGameSelect re-created the SelectBox which similarly
	// flipped gamesprocessed back to false via the panel's first-pass walk.
	ctx.world.lobby.gamesprocessed = false;
}

void LobbyClayScreen::ShowGameCreate(ScreenContext & ctx)
{
	silencer::ui::lobby_clay::GameCreatePanelInit(gameCreateState, ctx);
	gameCreateActive = true;
	gameJoinActive   = false;
	gameTechActive   = false;
	ctx.game.currentinterface = interfaceId;
}

void LobbyClayScreen::ShowGameJoin(ScreenContext & ctx)
{
	silencer::ui::lobby_clay::GameJoinPanelInit(gameJoinState);
	gameJoinActive   = true;
	gameCreateActive = false;
	gameTechActive   = false;
	ctx.game.currentinterface = interfaceId;
}

void LobbyClayScreen::ShowGameTech(ScreenContext & ctx)
{
	ctx.world.choosingtech = true;
	ctx.game.ShowTeamOverlays(false);
	ctx.world.RequestPeerList();

	silencer::ui::lobby_clay::GameTechPanelInit(gameTechState);
	gameTechActive   = true;
	gameJoinActive   = false;
	gameCreateActive = false;
	ctx.game.currentinterface = interfaceId;
}

void LobbyClayScreen::Draw(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;
	EnsureInitialized(dst.w, dst.h);

	float mx = 0.f, my = 0.f;
	Uint32 buttons = SDL_GetMouseState(&mx, &my);
	bool down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
	Clay_SetPointerState({ mx, my }, down);

	BankTextBeginFrame();
	BankButtonBeginFrame();
	ToggleBeginFrame();
	ScrollListBeginFrame();
	ScrollTextBoxBeginFrame();
	TextInputBeginFrame();
	silencer::ui::clay_inspector::BeginFrame();

	Clay_BeginLayout();
	BuildChromeTree(this, version, mapName, ctx.world.resources);
	silencer::ui::lobby_clay::BuildCharacterPanelTree(
		characterState, ctx.world, ctx.world.resources);
	silencer::ui::lobby_clay::BuildChatPanelTree(
		chatState, ctx.world, ctx.world.resources);
	if(!gameCreateActive && !gameJoinActive && !gameTechActive){
		silencer::ui::lobby_clay::BuildGameSelectPanelTree(
			gameSelectState, ctx.world.resources);
	}
	if(gameCreateActive){
		silencer::ui::lobby_clay::BuildGameCreatePanelTree(
			gameCreateState, ctx.world.resources);
	}
	if(gameJoinActive){
		silencer::ui::lobby_clay::BuildGameJoinPanelTree(
			gameJoinState, ctx.world.resources);
	}
	if(gameTechActive){
		silencer::ui::lobby_clay::BuildGameTechPanelTree(
			gameTechState, ctx.world, ctx.world.resources, *this);
	}
	Clay_RenderCommandArray cmds = Clay_EndLayout();

	Render(ctx.game, &dst, cmds);
}

void LobbyClayScreen::Destroy(ScreenContext & ctx)
{
	if(interfaceId){
		Interface * iface = static_cast<Interface *>(ctx.world.GetObjectFromId(interfaceId));
		if(iface) iface->DestroyInterface(ctx.world);
		interfaceId = 0;
	}
}

void LobbyClayScreen::SetMapNameOverlay(World & /*world*/, const char * name)
{
	mapName = name ? std::string(name).substr(0, 25) : std::string();
}

void LobbyClayScreen::SeedHostGameInfo(World & world, LobbyGame & lg)
{
	Serializer data;
	lg.Serialize(Serializer::WRITE, data);
	world.gameinfo.Serialize(Serializer::READ, data);
}

bool LobbyClayScreen::JoinPanelInLobby(World & world) const
{
	return world.gameplaystate == World::INLOBBY;
}

bool LobbyClayScreen::JoinPanelReadyBlocked(World & world) const
{
	Peer * localpeer = world.peerlist[world.localpeerid];
	return localpeer && localpeer->ishost && !world.AllPeersDownloadedMap();
}

void LobbyClayScreen::JoinPanelSendReady(World & world)
{
	Peer * localpeer = world.peerlist[world.localpeerid];
	bool ishost = localpeer && localpeer->ishost;
	if(!ishost || world.AllPeersDownloadedMap()){
		world.SendReady();
	}
}

void LobbyClayScreen::JoinPanelChangeTeam(World & world)
{
	world.ChangeTeam();
}

Uint8 LobbyClayScreen::TechPanelLocalPeerId(World & world) const
{
	return world.localpeerid;
}

Peer * LobbyClayScreen::TechPanelPeer(World & world, Uint8 peerid) const
{
	return world.peerlist[peerid];
}

void LobbyClayScreen::TechPanelRequestPeerList(World & world)
{
	world.RequestPeerList();
}

void LobbyClayScreen::TechPanelSetTech(World & world, Uint32 techchoices)
{
	world.SetTech(techchoices);
}

bool LobbyClayScreen::HandleBack(ScreenContext & ctx)
{
	if(gameJoinActive || gameTechActive){
		ctx.LeaveJoinedGame();
		SetMapNameOverlay(ctx.world, "");
		ShowGameSelect(ctx);
		return true;
	}
	if(gameCreateActive){
		ctx.world.lobby.gamesprocessed = false;
		ShowGameSelect(ctx);
		return true;
	}
	return false;
}
