#include "main_menu.h"

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
#include <string>

namespace ui {
namespace v2 {

namespace {

// Frame-local string arena. Clay captures Clay_String by (ptr, length) —
// the chars must outlive Clay_EndLayout(). RenderMainMenu resets the
// offset at the top of every call; pointers handed to CLAY_TEXT() and
// Clay_GetElementId() in this frame stay valid through EndLayout.
constexpr size_t kFrameStringBytes = 1024;
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

// Clay_OnHover callback. userData is the address of a std::function<void()>
// owned by the MainMenuHandlers struct passed into RenderMainMenu — that
// struct outlives Clay_EndLayout() (it's a frame-local on the caller's
// stack). The handler fires on the PRESSED_THIS_FRAME edge so a click
// triggers exactly once even if the user holds the button.
void OnButtonClick(Clay_ElementId, Clay_PointerData p, intptr_t user) {
	if(p.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
	auto * h = reinterpret_cast<const std::function<void()> *>(user);
	if(h && *h) (*h)();
}

// Menu button: fixed 196×33, hover-tinted background (palette indices
// from theme.h), centered text label, fires `handler` on click. The
// stable Clay id is derived from the label so the button hashes back to
// the same element across frames (required for Clay_Hovered to consult
// last frame's pointerOverIds list).
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

void RenderMainMenu(const Context & ctx, const MainMenuHandlers & h) {
	g_frame_off = 0;

	CLAY({
		.id = CLAY_ID("MainMenuRoot"),
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
			.childGap        = 8,
			.childAlignment  = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = ::ui::kColorPanelBg,
	}) {
		MenuButton("Tutorial",         h.on_tutorial);
		MenuButton("Connect To Lobby", h.on_lobby);
		MenuButton("Options",          h.on_options);
		MenuButton("Exit",             h.on_exit);
	}

	if(ctx.version){
		std::string vstr = std::string("Silencer v") + ctx.version;
		CLAY({
			.id = CLAY_ID("MainMenuVersion"),
			.floating = {
				.offset       = { 10.0f, (float)(ctx.logical_h - 17) },
				.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
				.attachTo     = CLAY_ATTACH_TO_ROOT,
			},
		}) {
			CLAY_TEXT(FrameStr(vstr.c_str()), CLAY_TEXT_CONFIG(::ui::kFontBody));
		}
	}
}

// -----------------------------------------------------------------------------
// MainMenuRuntime — engine wire-in for GameState::MAINMENU.
// -----------------------------------------------------------------------------

namespace {

MainMenuHandlers BuildMainMenuHandlers(ScreenContext & sctx){
	MainMenuHandlers h;
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
	ctx.dt      = dt;

	target.Clear(0);

	EnsureClayContext(ctx);
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/false);
	Clay_UpdateScrollContainers(/*drag=*/false, Clay_Vector2{ 0.0f, 0.0f }, dt);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	MainMenuHandlers handlers = BuildMainMenuHandlers(sctx_);
	RenderMainMenu(ctx, handlers);
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	::ui::DrawRenderCommands(cmds, renderer, target, scale);
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

	// Lay out a fresh tree this frame and register OnHover callbacks
	// against `handlers` on THIS stack frame, then fire the click via
	// Clay_SetPointerState with pointer_down=true. SetPointerState
	// walks the just-finalised tree synchronously and invokes the
	// OnHover callbacks while `handlers` is still alive. We can't
	// reuse the prior Render frame's tree — its OnHover userData
	// pointed at a handlers struct that has already been destroyed.
	EnsureClayContext(ctx);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	MainMenuHandlers handlers = BuildMainMenuHandlers(sctx_);
	RenderMainMenu(ctx, handlers);
	(void)Clay_EndLayout();
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/true);
	return true;
}

}  // namespace v2
}  // namespace ui
