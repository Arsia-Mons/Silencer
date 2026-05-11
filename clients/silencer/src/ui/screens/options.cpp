#include "options.h"

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

namespace ui {
namespace v2 {

Node BuildOptions(const Context & ctx, const OptionsHandlers & handlers)
{
	(void)ctx;
	// Absolute `.at()` mirrors legacy OptionsScreen::Build exactly. The four
	// buttons stack at evenly-spaced y values (52 px between anchors, with
	// chrome height 33 → 19 px visible gap) so a VStack with .withGap(19)
	// could reproduce the *spacing*, but it can't reproduce the *anchor*
	// origin without baking in the sprite offset for bank-6 index-7 — and
	// that offset is loaded at runtime from PALETTE/sprite metadata. Per
	// the design doc's escape hatch, falling back to absolute keeps the
	// port pixel-identical with no runtime fragility.
	return Background(/*bank=*/6, /*index=*/0, {
		Button("Controls").at(-89, -142).onClick(handlers.on_controls),
		Button("Display") .at(-89,  -90).onClick(handlers.on_display),
		Button("Audio")   .at(-89,  -38).onClick(handlers.on_audio),
		Button("Go Back") .at(-89,   15).onClick(handlers.on_go_back),
	});
}

// -----------------------------------------------------------------------------
// OptionsRuntime — engine wire-in for GameState::OPTIONS.
// -----------------------------------------------------------------------------

namespace {

OptionsHandlers BuildOptionsHandlers(ScreenContext & sctx){
	OptionsHandlers h;
	h.on_controls = [&sctx](){ sctx.GoToState(GameState::OPTIONSCONTROLS); };
	h.on_display  = [&sctx](){ sctx.GoToState(GameState::OPTIONSDISPLAY); };
	h.on_audio    = [&sctx](){ sctx.GoToState(GameState::OPTIONSAUDIO); };
	h.on_go_back  = [&sctx](){ sctx.GoToState(GameState::MAINMENU); };
	return h;
}

}  // namespace

OptionsRuntime::OptionsRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

void OptionsRuntime::Render(Surface & target, ::Renderer & renderer,
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

	OptionsHandlers handlers = BuildOptionsHandlers(sctx_);
	target.Clear(0);
	state_.BeginFrame();
	Node tree = BuildOptions(ctx, handlers);
	::ui::v2::EnsureClayContext(ctx);
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/false);
	Clay_UpdateScrollContainers(/*drag=*/false, Clay_Vector2{ 0.0f, 0.0f }, dt);
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
	state_.EndFrame();
}

bool OptionsRuntime::DispatchMouseDown(int mouse_x, int mouse_y,
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
	OptionsHandlers handlers = BuildOptionsHandlers(sctx_);
	Node tree = BuildOptions(ctx, handlers);
	Layout(tree, ctx);
	DispatchClick(tree, ctx);
	return true;
}

}  // namespace v2
}  // namespace ui
