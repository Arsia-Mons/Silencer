#include "options_display.h"

#include "context.h"
#include "layout.h"
#include "render_commands.h"
#include "theme.h"

#include "config.h"
#include "game.h"
#include "game_state.h"
#include "renderdevice.h"
#include "screen_context.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <SDL3/SDL_video.h>
#include <cstring>

namespace ui {
namespace v2 {

namespace {

// Frame-local string arena. Same shape as main_menu / options — reset
// at the top of RenderOptionsDisplay; buffer survives through Clay_EndLayout.
constexpr size_t kFrameStringBytes = 256;
thread_local char   g_frame_strings[kFrameStringBytes];
thread_local size_t g_frame_off = 0;

Clay_String FrameStr(const char * s) {
	size_t n = std::strlen(s);
	if(g_frame_off + n + 1 > kFrameStringBytes) g_frame_off = 0;
	char * p = &g_frame_strings[g_frame_off];
	std::memcpy(p, s, n);
	p[n] = '\0';
	g_frame_off += n + 1;
	Clay_String out{};
	out.length = (int32_t)n;
	out.chars  = p;
	return out;
}

void OnButtonClick(Clay_ElementId, Clay_PointerData p, intptr_t user) {
	if(p.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
	auto * h = reinterpret_cast<const std::function<void()> *>(user);
	if(h && *h) (*h)();
}

void MenuButton(const char * key, const char * label, const std::function<void()> & handler) {
	Clay_String key_s   = FrameStr(key);
	Clay_String label_s = FrameStr(label);
	CLAY({
		.id = Clay_GetElementId(key_s),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(33) },
			.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = Clay_Hovered() ? ::ui::kColorSelectHi : ::ui::kColorScrollbarFill,
		.cornerRadius    = ::ui::kCornerSmall,
	}) {
		Clay_OnHover(OnButtonClick, (intptr_t)&handler);
		CLAY_TEXT(label_s, CLAY_TEXT_CONFIG(::ui::kFontTitle));
	}
}

}  // namespace

void RenderOptionsDisplay(const Context & ctx, const OptionsDisplayHandlers & h, const OptionsDisplayState & state) {
	(void)ctx;
	g_frame_off = 0;

	CLAY({
		.id = CLAY_ID("OptionsDisplayRoot"),
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
			.childGap        = 8,
			.childAlignment  = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = ::ui::kColorPanelBg,
	}) {
		CLAY({ .id = CLAY_ID("OptionsDisplayTitle") }) {
			CLAY_TEXT(FrameStr("Display Options"), CLAY_TEXT_CONFIG(::ui::kFontTitle));
		}
		MenuButton("fullscreen",     state.fullscreen  ? "Fullscreen: On"     : "Fullscreen: Off",     h.on_toggle_fullscreen);
		MenuButton("smooth_scaling", state.scalefilter ? "Smooth Scaling: On" : "Smooth Scaling: Off", h.on_toggle_smooth_scaling);
		MenuButton("save",           "Save",           h.on_save);
		MenuButton("cancel",         "Cancel",         h.on_cancel);
	}
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
	ctx.dt      = dt;

	target.Clear(0);

	EnsureClayContext(ctx);
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/false);
	Clay_UpdateScrollContainers(/*drag=*/false, Clay_Vector2{ 0.0f, 0.0f }, dt);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	OptionsDisplayHandlers handlers = BuildOptionsDisplayHandlers(sctx_);
	OptionsDisplayState live = CurrentOptionsDisplay();
	RenderOptionsDisplay(ctx, handlers, live);
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	::ui::DrawRenderCommands(cmds, renderer, target, scale);
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

	// Re-lay out so OnHover userData points at `handlers` on this stack
	// frame; the prior Render frame's tree captured a now-destroyed
	// handlers struct. SetPointerState walks the just-finalised tree.
	EnsureClayContext(ctx);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	OptionsDisplayHandlers handlers = BuildOptionsDisplayHandlers(sctx_);
	OptionsDisplayState live = CurrentOptionsDisplay();
	RenderOptionsDisplay(ctx, handlers, live);
	(void)Clay_EndLayout();
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/true);
	return true;
}

}  // namespace v2
}  // namespace ui
