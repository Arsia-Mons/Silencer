#include "main_menu.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"

#include "game.h"
#include "game_state.h"
#include "screen_context.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <string>

namespace ui {
namespace v2 {

Node BuildMainMenu(const Context & ctx, const MainMenuHandlers & handlers)
{
	std::string version_label = "Silencer v";
	if(ctx.version) version_label += ctx.version;

	// MainMenu uses absolute `.at()` positions matching the legacy
	// MainMenuScreen exactly. The staggered button arrangement (per-row
	// horizontal offset, logo overlapping the column on the left) is not
	// naturally container-shaped, so the design doc's `.at()` escape
	// hatch is the right tool. The Layout pass detects no containers in
	// this subtree and writes no rects → render+dispatch fall through to
	// the absolute path, producing byte-identical output to legacy.
	//
	// When containers do land (Options, LobbyConnect, etc.) they emit
	// Clay scopes and the rect-driven path takes over for those screens.
	return Background(/*bank=*/6, /*index=*/0, {
		// Bank 208's animation table (Overlay::Tick case 208) ramps frames
		// 29..58, holds at 60 for ~1 s, then ramps back. Index 60 is the
		// steady-state "fully revealed" logo — what users see most of the
		// time. Index 0 has a different, pre-animation frame with stale
		// .enabled / www.won.net text baked in.
		Sprite(/*bank=*/208, /*index=*/60),
		Label(version_label, /*font_bank=*/133, /*font_width=*/11)
			.at(10, (Sint16)(ctx.logical_h - 17)),
		Button("Tutorial").at(40, -134).onClick(handlers.on_tutorial),
		Button("Connect To Lobby").at(80, -67).onClick(handlers.on_lobby),
		Button("Options").at(40, 0).onClick(handlers.on_options),
		Button("Exit").at(0, 67).onClick(handlers.on_exit),
	});
}

// -----------------------------------------------------------------------------
// MainMenuRuntime — engine wire-in for GameState::MAINMENU.
// -----------------------------------------------------------------------------

namespace {

MainMenuHandlers BuildMainMenuHandlers(ScreenContext & sctx){
	MainMenuHandlers h;
	// Bound to real state transitions matching the legacy MainMenuScreen::Tick
	// switch (BTN_TUTORIAL→SINGLEPLAYERGAME, BTN_LOBBY→LOBBYCONNECT,
	// BTN_OPTIONS→OPTIONS, BTN_EXIT→quit).
	h.on_tutorial = [&sctx](){ sctx.GoToState(GameState::SINGLEPLAYERGAME); };
	h.on_lobby    = [&sctx](){ sctx.GoToState(GameState::LOBBYCONNECT); };
	h.on_options  = [&sctx](){ sctx.GoToState(GameState::OPTIONS); };
	h.on_exit     = [&sctx](){ sctx.RequestQuit(); };
	return h;
}

}  // namespace

MainMenuRuntime::MainMenuRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

void MainMenuRuntime::Render(Surface & target, ::Renderer & renderer,
                              int mouse_x, int mouse_y, float dt,
                              int logical_w, int logical_h, int scale){
	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	ctx.dt      = dt;

	MainMenuHandlers handlers = BuildMainMenuHandlers(sctx_);
	target.Clear(0);
	state_.BeginFrame();
	Node tree = BuildMainMenu(ctx, handlers);
	::ui::v2::EnsureClayContext(ctx);
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/false);
	Clay_UpdateScrollContainers(/*drag=*/false, Clay_Vector2{ 0.0f, 0.0f }, dt);
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
	state_.EndFrame();
}

bool MainMenuRuntime::DispatchMouseDown(int mouse_x, int mouse_y,
                                int logical_w, int logical_h, int scale){
	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	MainMenuHandlers handlers = BuildMainMenuHandlers(sctx_);
	Node tree = BuildMainMenu(ctx, handlers);
	Layout(tree, ctx);
	DispatchClick(tree, ctx);
	return true;
}

}  // namespace v2
}  // namespace ui
