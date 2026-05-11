#include "options_display.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"

#include "config.h"
#include "game.h"
#include "game_state.h"
#include "renderdevice.h"
#include "screen_context.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <SDL3/SDL_video.h>
#include <string>

namespace ui {
namespace v2 {

Node BuildOptionsDisplay(const Context & ctx, const OptionsDisplayHandlers & handlers, const OptionsDisplayState * state)
{
	(void)ctx;
	// Title text: "Display Options" centered at y=14. Legacy formula:
	//   x = 320 - (len * textwidth) / 2
	// with textwidth=12, len=15 → x = 320 - 90 = 230.
	const std::string title = "Display Options";
	const int title_x = 320 - ((int)title.size() * 12) / 2;

	// Two toggle rows. Legacy spacing: row y = 50 + i*53 (button anchor),
	// indicator y = 137 + i*53 (Overlay top-left, no chrome offset since
	// off/on sprites are bare Overlays in the legacy path). When `state`
	// is null, indices match the build-time defaults that the preview
	// PPM diff is locked to; when provided, they follow the legacy Tick
	// derivation.
	auto row = [](int i, const char * label, std::function<void()> handler, int off_idx, int on_idx) {
		const int by = 50 + i * 53;
		const int oy = 137 + i * 53;
		return Group({
			Button(label, ButtonType::B220x33).at(100, (Sint16)by).onClick(std::move(handler)),
			Sprite(6, off_idx).at(420, (Sint16)oy),
			Sprite(6, on_idx).at(450, (Sint16)oy),
		});
	};

	const int fs_off  = state ? (state->fullscreen ? 12 : 13)   : 12;
	const int fs_on   = state ? (state->fullscreen ? 15 : 14)   : 14;
	const int ss_off  = state ? (state->scalefilter ? 12 : 13)  : 12;
	const int ss_on   = state ? (state->scalefilter ? 15 : 14)  : 14;

	return Background(/*bank=*/6, /*index=*/0, {
		Label(title, /*font_bank=*/135, /*font_width=*/12).at((Sint16)title_x, 14),
		row(0, "Fullscreen",     handlers.on_toggle_fullscreen,     fs_off, fs_on),
		row(1, "Smooth Scaling", handlers.on_toggle_smooth_scaling, ss_off, ss_on),
		Button("Save")  .at(-200, 117).onClick(handlers.on_save),
		Button("Cancel").at(  20, 117).onClick(handlers.on_cancel),
	});
}

// -----------------------------------------------------------------------------
// OptionsDisplayRuntime — engine wire-in for GameState::OPTIONSDISPLAY.
// -----------------------------------------------------------------------------

namespace {

OptionsDisplayHandlers BuildOptionsDisplayHandlers(ScreenContext & sctx){
	OptionsDisplayHandlers h;
	h.on_toggle_fullscreen = [&sctx](){
		Config & cfg = Config::GetInstance();
		cfg.fullscreen = !cfg.fullscreen;
		if(sctx.window) SDL_SetWindowFullscreen(sctx.window, cfg.fullscreen);
	};
	h.on_toggle_smooth_scaling = [&sctx](){
		Config & cfg = Config::GetInstance();
		cfg.scalefilter = !cfg.scalefilter;
		if(sctx.renderdevice) sctx.renderdevice->SetScaleFilter(cfg.scalefilter);
	};
	h.on_save = [&sctx](){
		Config::GetInstance().Save();
		sctx.GoToState(GameState::OPTIONS);
	};
	h.on_cancel = [&sctx](){
		Config & cfg = Config::GetInstance();
		cfg.Load();
		if(sctx.renderdevice) sctx.renderdevice->SetScaleFilter(cfg.scalefilter);
		if(sctx.window) SDL_SetWindowFullscreen(sctx.window, cfg.fullscreen);
		sctx.GoToState(GameState::OPTIONS);
	};
	return h;
}

OptionsDisplayState CurrentOptionsDisplay(){
	OptionsDisplayState s;
	Config & cfg = Config::GetInstance();
	s.fullscreen  = cfg.fullscreen;
	s.scalefilter = cfg.scalefilter;
	return s;
}

}  // namespace

OptionsDisplayRuntime::OptionsDisplayRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

void OptionsDisplayRuntime::Render(Surface & target, ::Renderer & renderer,
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

	OptionsDisplayHandlers handlers = BuildOptionsDisplayHandlers(sctx_);
	OptionsDisplayState live = CurrentOptionsDisplay();
	target.Clear(0);
	state_.BeginFrame();
	Node tree = BuildOptionsDisplay(ctx, handlers, &live);
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
	state_.EndFrame();
}

bool OptionsDisplayRuntime::DispatchMouseDown(int mouse_x, int mouse_y,
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
	OptionsDisplayHandlers handlers = BuildOptionsDisplayHandlers(sctx_);
	OptionsDisplayState live = CurrentOptionsDisplay();
	Node tree = BuildOptionsDisplay(ctx, handlers, &live);
	Layout(tree, ctx);
	DispatchClick(tree, ctx);
	return true;
}

}  // namespace v2
}  // namespace ui
