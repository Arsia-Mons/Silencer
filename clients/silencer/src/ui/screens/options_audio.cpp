#include "options_audio.h"

#include "context.h"
#include "layout.h"
#include "render_commands.h"
#include "theme.h"

#include "audio.h"
#include "config.h"
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

// Frame-local string arena. Same shape as main_menu / options — reset
// at the top of RenderOptionsAudio; buffer survives through Clay_EndLayout.
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

void RenderOptionsAudio(const Context & ctx, const OptionsAudioHandlers & h, bool music_on) {
	(void)ctx;
	g_frame_off = 0;

	CLAY({
		.id = CLAY_ID("OptionsAudioRoot"),
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
			.childGap        = 8,
			.childAlignment  = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = ::ui::kColorPanelBg,
	}) {
		CLAY({ .id = CLAY_ID("OptionsAudioTitle") }) {
			CLAY_TEXT(FrameStr("Audio Options"), CLAY_TEXT_CONFIG(::ui::kFontTitle));
		}
		MenuButton("music",  music_on ? "Music: On" : "Music: Off", h.on_toggle_music);
		MenuButton("save",   "Save",                                h.on_save);
		MenuButton("cancel", "Cancel",                              h.on_cancel);
	}
}

// -----------------------------------------------------------------------------
// OptionsAudioRuntime — engine wire-in for GameState::OPTIONSAUDIO.
// -----------------------------------------------------------------------------

namespace {

OptionsAudioHandlers BuildOptionsAudioHandlers(ScreenContext & sctx){
	OptionsAudioHandlers h;
	h.on_toggle_music = [](){
		Config & cfg = Config::GetInstance();
		cfg.music = !cfg.music;
		if(cfg.music) Audio::GetInstance().ResumeMusic();
		else          Audio::GetInstance().PauseMusic();
	};
	h.on_save = [&sctx](){
		Config::GetInstance().Save();
		sctx.GoToState(GameState::OPTIONS);
	};
	h.on_cancel = [&sctx](){
		Config & cfg = Config::GetInstance();
		cfg.Load();
		if(cfg.music) Audio::GetInstance().ResumeMusic();
		else          Audio::GetInstance().PauseMusic();
		sctx.GoToState(GameState::OPTIONS);
	};
	return h;
}

}  // namespace

OptionsAudioRuntime::OptionsAudioRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

void OptionsAudioRuntime::Render(Surface & target, ::Renderer & renderer,
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
	OptionsAudioHandlers handlers = BuildOptionsAudioHandlers(sctx_);
	RenderOptionsAudio(ctx, handlers, Config::GetInstance().music);
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	::ui::DrawRenderCommands(cmds, renderer, target, scale);
}

bool OptionsAudioRuntime::DispatchMouseDown(int mouse_x, int mouse_y,
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
	OptionsAudioHandlers handlers = BuildOptionsAudioHandlers(sctx_);
	RenderOptionsAudio(ctx, handlers, Config::GetInstance().music);
	(void)Clay_EndLayout();
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/true);
	return true;
}

}  // namespace v2
}  // namespace ui
