#include "lobby_clay_screen.h"

#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "interface.h"
#include "objecttypes.h"
#include "renderer.h"
#include "resources.h"
#include "surface.h"
#include "game_create_panel.h"
#include "game_join_panel.h"
#include "game_tech_panel.h"
#include "lobbygame.h"
#include "serializer.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/scroll_list.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text_input.h"
#include "primitives/toggle.h"
#include "clay_character_panel.h"
#include "clay_chat_panel.h"
#include "clay_game_select_panel.h"
#include "clay_game_create_panel.h"

#include <SDL3/SDL.h>

#include <memory>

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
// floating .offset keeps the on-screen pixels byte-identical to the legacy
// path.
void BuildChromeTree(LobbyClayScreen * screen,
                     const std::string & version,
                     const std::string & mapName,
                     Resources & resources)
{
	using namespace silencer::clay_bridge;

	const int W = 640;
	const int H = 480;

	// Compensate for sprite anchor offsets — matches the legacy
	// `object->x - spriteoffsetx[bank][index]` formula.
	const int bgOffX = -resources.spriteoffsetx[7][1];
	const int bgOffY = -resources.spriteoffsety[7][1];
	const int btnOffX = 473 - resources.spriteoffsetx[7][24];
	const int btnOffY = 29  - resources.spriteoffsety[7][24];

	CLAY({ .id = CLAY_ID("LobbyClayRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	       } }) {
		// Fullscreen background sprite (bank 7 idx 1).
		CLAY({ .id = CLAY_ID("LobbyBg"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
		       },
		       .image = { .imageData = PackImage(7, 1) },
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset = { (float)bgOffX, (float)bgOffY } } }) {}

		// "Silencer" title at (15, 32). Bank 135 / w11 / eff=152 — matches
		// LobbyScreen::Build's `toptext` overlay.
		CLAY({ .id = CLAY_ID("LobbyTitle"),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset = { 15, 32 } } }) {
			BankText(CLAY_STRING("Silencer"),
			         BankTextVariant::Title,
			         { .effectColor = 152 });
		}

		// "v.X" version at (115, 39). Bank 133 / w6 / eff=189 — matches
		// `vertext`.
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

		// Map-name overlay at (180, 32). Bank 135 / w11 / eff=129 /
		// brightness=160 (128+32) / colorRamp=on — matches `mapnametext`.
		// Skip emission when empty so the bridge doesn't dispatch a
		// zero-length text command.
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

		// "Go Back" B156x21 chrome button at (473, 29). Matches
		// `exitbutton`. Offset compensates for spriteoffsetx[7][24]
		// (the chrome sprite anchor), the same compensation the legacy
		// renderer applies on the way to BlitSurface.
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
	}
}
}  // namespace

void LobbyClayScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;
	ctx.ResetPresentation(2);
	ctx.renderer.camera.SetPosition(320, 240);

	// Version string is immutable at runtime — cache once.
	version  = "v.";
	version += world.GetVersion();
	mapName.clear();
	goBackClicked = false;

	// Parent Interface for the legacy panels. The chrome elements that
	// LobbyScreen::Build creates here (bg overlay, title, version,
	// mapname, Go Back button) are NOT created — the Clay tree paints
	// them in Draw each frame instead.
	Interface * lobbyiface = static_cast<Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
	interfaceId = lobbyiface->id;

	// P12: character panel runs in Clay. We deliberately DO NOT call
	// `character.Build(ctx, lobbyiface)` — that would create the legacy
	// world Overlay/Toggle objects which would then double-render on top
	// of the Clay subtree. The inherited `character` member's interfaceId
	// stays 0 so its Tick is a no-op.
	silencer::ui::lobby_clay::CharacterPanelInit(characterState);

	// P13: chat panel runs in Clay. Skip the legacy chat.Build for the
	// same reason character.Build was skipped — its world Overlay/TextBox
	// /TextInput/ScrollBar objects would double-render on top of the
	// Clay subtree. Legacy `chat` member's interfaceId stays 0 → its
	// Tick (invoked via LobbyScreen::Tick) early-returns harmlessly.
	silencer::ui::lobby_clay::ChatPanelInit(chatState);

	// P14: GameSelect panel runs in Clay. Skip legacy `gameSelect->Build`.
	// The legacy `gameSelect` unique_ptr stays null; LobbyScreen::Tick's
	// `if(gameSelect)` and the `gameSelect || gameCreate` gates handle the
	// null case (the gates that need to fire when there's no right-side
	// legacy panel are kept alive by gameCreate/gameJoin/gameTech presence
	// when those panels are active).
	silencer::ui::lobby_clay::GameSelectPanelInit(gameSelectState);
}

void LobbyClayScreen::Tick(ScreenContext & ctx)
{
	// Chrome Go Back — flag was set by the Clay onClick trampoline on the
	// previous frame. Consume it before delegating to the base lobby pump.
	if(goBackClicked){
		goBackClicked = false;
		if(ctx.game.GoBack()) return;
	}

	// Reconcile any agency-toggle click from the previous frame's Clay
	// layout into Config + world. Domain glue (Config::Save, SetAgency)
	// lives here in the screen — the Toggle primitive only reports clicks.
	silencer::ui::lobby_clay::CharacterPanelTick(characterState, ctx.world);

	// Drain lobby chat / presence / channel changes into Clay state.
	silencer::ui::lobby_clay::ChatPanelTick(chatState, ctx.world);

	// Consume the Clay GameSelect panel's per-frame click flags + rebuild
	// rows on lobby updates. Only active when no other right-side panel
	// owns the surface — when gameCreateActive/gameJoin/gameTech are live,
	// the games-list Clay tree isn't emitted, so its click flags can't fire.
	if(!gameCreate && !gameCreateActive && !gameJoin && !gameTech){
		silencer::ui::lobby_clay::GameSelectPanelTick(
			gameSelectState, ctx.world, ctx, *this);
	}

	// Clay GameCreate surface owns its own deferred-create state machine
	// (mirrors LobbyScreen::Tick's `if(gameCreate)` block + the create-
	// button kickoff that used to live in GameCreatePanel::Tick).
	if(gameCreateActive){
		silencer::ui::lobby_clay::GameCreatePanelTick(
			gameCreateState, ctx.world, ctx, *this);
	}

	// Base class drives the rest: panel ticks, deferred CreateGame state
	// machine, CONNECTED → GameJoin handoff, disconnect detection. The
	// chrome-button scan inside LobbyScreen::Tick finds no Go Back button
	// in our interface (we didn't create one) and is a harmless no-op.
	LobbyScreen::Tick(ctx);
}

void LobbyClayScreen::ShowGameSelect(ScreenContext & ctx)
{
	// Tear down whichever legacy right-side panel is currently active. The
	// games-list Clay surface is always present in Draw and reappears once
	// these are gone — no legacy `gameSelect = new GameSelectPanel(...)`
	// rebuild required.
	Interface * lobbyiface = static_cast<Interface *>(
		ctx.world.GetObjectFromId(interfaceId));
	auto destroy = [&](Uint16 id) {
		Interface * panelIface = static_cast<Interface *>(
			ctx.world.GetObjectFromId(id));
		if(panelIface) panelIface->DestroyInterface(ctx.world, lobbyiface);
	};
	if(gameCreate){ destroy(gameCreate->interfaceId); gameCreate.reset(); }
	if(gameJoin)  { destroy(gameJoin->interfaceId);   gameJoin.reset();   }
	if(gameTech)  {
		destroy(gameTech->interfaceId);
		gameTech.reset();
		ctx.world.choosingtech = false;
		ctx.game.ShowTeamOverlays(true);
	}
	gameCreateActive = false;
	// Force the games-list to re-snapshot from world.lobby on the next Tick
	// — the legacy ShowGameSelect re-created the SelectBox which similarly
	// flipped gamesprocessed back to false via the panel's first-pass walk.
	ctx.world.lobby.gamesprocessed = false;
}

void LobbyClayScreen::ShowGameCreate(ScreenContext & ctx)
{
	// Tear down any legacy right-side panel that was alive, then activate
	// the Clay game-create surface. The legacy `gameCreate` unique_ptr
	// stays null — `gameCreateActive` is the equivalent gate for the Clay
	// path's Draw + Tick.
	Interface * lobbyiface = static_cast<Interface *>(
		ctx.world.GetObjectFromId(interfaceId));
	auto destroy = [&](Uint16 id) {
		Interface * panelIface = static_cast<Interface *>(
			ctx.world.GetObjectFromId(id));
		if(panelIface) panelIface->DestroyInterface(ctx.world, lobbyiface);
	};
	if(gameCreate){ destroy(gameCreate->interfaceId); gameCreate.reset(); }
	if(gameJoin)  { destroy(gameJoin->interfaceId);   gameJoin.reset();   }
	if(gameTech)  {
		destroy(gameTech->interfaceId);
		gameTech.reset();
		ctx.world.choosingtech = false;
		ctx.game.ShowTeamOverlays(true);
	}
	silencer::ui::lobby_clay::GameCreatePanelInit(gameCreateState, ctx);
	gameCreateActive = true;
	ctx.game.currentinterface = interfaceId;
}

void LobbyClayScreen::Draw(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;
	EnsureInitialized(dst.w, dst.h);

	// Pointer state from SDL. Headless/dedicated mode returns (0, 0) with
	// no buttons pressed; the Clay onClick callbacks won't fire which is
	// the same behavior as the legacy mouse path.
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

	Clay_BeginLayout();
	BuildChromeTree(this, version, mapName, ctx.world.resources);
	silencer::ui::lobby_clay::BuildCharacterPanelTree(
		characterState, ctx.world, ctx.world.resources);
	silencer::ui::lobby_clay::BuildChatPanelTree(
		chatState, ctx.world, ctx.world.resources);
	// GameSelect tree is mutually exclusive with any right-side panel
	// (Clay gameCreate or legacy gameJoin/gameTech). When one of those is
	// active, suppress the games-list emission so it owns the right column.
	if(!gameCreate && !gameCreateActive && !gameJoin && !gameTech){
		silencer::ui::lobby_clay::BuildGameSelectPanelTree(
			gameSelectState, ctx.world.resources);
	}
	if(gameCreateActive){
		silencer::ui::lobby_clay::BuildGameCreatePanelTree(
			gameCreateState, ctx.world.resources);
	}
	Clay_RenderCommandArray cmds = Clay_EndLayout();

	Render(ctx.game, &dst, cmds);
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

bool LobbyClayScreen::HandleBack(ScreenContext & ctx)
{
	// Clay GameCreate surface → swap back to games list (mirrors the
	// legacy LobbyScreen::HandleBack's `if(gameCreate)` branch).
	if(gameCreateActive){
		ctx.world.lobby.gamesprocessed = false;
		ShowGameSelect(ctx);
		return true;
	}
	return LobbyScreen::HandleBack(ctx);
}
