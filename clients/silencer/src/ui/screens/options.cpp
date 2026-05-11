#include "options.h"

#include "context.h"
#include "layout.h"
#include "render_commands.h"
#include "theme.h"

#include "game.h"
#include "game_state.h"
#include "screen_context.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <cstring>

namespace ui {
namespace v2 {

namespace {

// Frame-local string arena. Same shape as main_menu — reset at the top
// of RenderOptions; buffer survives through Clay_EndLayout.
constexpr size_t kFrameStringBytes = 512;
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

void MenuButton(const char * label, const std::function<void()> & handler) {
	Clay_String label_s = FrameStr(label);
	CLAY({
		.id = Clay_GetElementId(label_s),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED(196), CLAY_SIZING_FIXED(33) },
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

void RenderOptions(const Context & ctx, const OptionsHandlers & h) {
	(void)ctx;
	g_frame_off = 0;

	CLAY({
		.id = CLAY_ID("OptionsRoot"),
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
			.childGap        = 8,
			.childAlignment  = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = ::ui::kColorPanelBg,
	}) {
		MenuButton("Controls", h.on_controls);
		MenuButton("Display",  h.on_display);
		MenuButton("Audio",    h.on_audio);
		MenuButton("Go Back",  h.on_go_back);
	}
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
	ctx.dt      = dt;

	target.Clear(0);

	EnsureClayContext(ctx);
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/false);
	Clay_UpdateScrollContainers(/*drag=*/false, Clay_Vector2{ 0.0f, 0.0f }, dt);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	OptionsHandlers handlers = BuildOptionsHandlers(sctx_);
	RenderOptions(ctx, handlers);
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	::ui::DrawRenderCommands(cmds, renderer, target, scale);
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

	// Re-lay out so OnHover userData points at `handlers` on this stack
	// frame; the prior Render frame's tree captured a now-destroyed
	// handlers struct. SetPointerState walks the just-finalised tree.
	EnsureClayContext(ctx);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	OptionsHandlers handlers = BuildOptionsHandlers(sctx_);
	RenderOptions(ctx, handlers);
	(void)Clay_EndLayout();
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/true);
	return true;
}

}  // namespace v2
}  // namespace ui
