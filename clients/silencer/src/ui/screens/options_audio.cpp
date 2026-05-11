#include "options_audio.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"

#include "audio.h"
#include "config.h"
#include "game.h"
#include "game_state.h"
#include "screen_context.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <string>

namespace ui {
namespace v2 {

Node BuildOptionsAudio(const Context & ctx, const OptionsAudioHandlers & handlers, const OptionsAudioState * state)
{
	(void)ctx;
	const std::string title = "Audio Options";
	const int title_x = 320 - ((int)title.size() * 12) / 2;

	auto row = [](int i, const char * label, std::function<void()> handler, int off_idx, int on_idx) {
		const int by = 50 + i * 53;
		const int oy = 137 + i * 53;
		return Group({
			Button(label, ButtonType::B220x33).at(100, (Sint16)by).onClick(std::move(handler)),
			Sprite(6, off_idx).at(420, (Sint16)oy),
			Sprite(6, on_idx).at(450, (Sint16)oy),
		});
	};

	const int mu_off = state ? (state->music ? 12 : 13) : 12;
	const int mu_on  = state ? (state->music ? 15 : 14) : 14;

	return Background(/*bank=*/6, /*index=*/0, {
		Label(title, /*font_bank=*/135, /*font_width=*/12).at((Sint16)title_x, 14),
		row(0, "Music", handlers.on_toggle_music, mu_off, mu_on),
		Button("Save")  .at(-200, 117).onClick(handlers.on_save),
		Button("Cancel").at(  20, 117).onClick(handlers.on_cancel),
	});
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

OptionsAudioState CurrentOptionsAudio(){
	OptionsAudioState s;
	s.music = Config::GetInstance().music;
	return s;
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
	ctx.state   = &state_;
	ctx.dt      = dt;

	OptionsAudioHandlers handlers = BuildOptionsAudioHandlers(sctx_);
	OptionsAudioState live = CurrentOptionsAudio();
	target.Clear(0);
	state_.BeginFrame();
	Node tree = BuildOptionsAudio(ctx, handlers, &live);
	::ui::v2::EnsureClayContext(ctx);
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/false);
	Clay_UpdateScrollContainers(/*drag=*/false, Clay_Vector2{ 0.0f, 0.0f }, dt);
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
	state_.EndFrame();
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
	ctx.state   = &state_;
	OptionsAudioHandlers handlers = BuildOptionsAudioHandlers(sctx_);
	OptionsAudioState live = CurrentOptionsAudio();
	Node tree = BuildOptionsAudio(ctx, handlers, &live);
	Layout(tree, ctx);
	DispatchClick(tree, ctx);
	return true;
}

}  // namespace v2
}  // namespace ui
