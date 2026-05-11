#include "update.h"

#include "context.h"
#include "layout.h"
#include "render_commands.h"
#include "theme.h"

#include "game.h"
#include "game_state.h"
#include "screen_context.h"
#include "updater.h"
#include "updaterstage2.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace ui {
namespace v2 {

namespace {

// Frame-local string arena. Same shape as main_menu / options / options_audio
// — reset at the top of RenderUpdate; buffer survives through Clay_EndLayout.
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

void RenderUpdate(const Context & ctx, const UpdateHandlers & h,
                  const UpdateState & state) {
	(void)ctx;
	g_frame_off = 0;

	CLAY({
		.id = CLAY_ID("UpdateRoot"),
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
			.childGap        = 8,
			.childAlignment  = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = ::ui::kColorPanelBg,
	}) {
		if(!state.status_text.empty()){
			CLAY({ .id = CLAY_ID("UpdateStatus") }) {
				CLAY_TEXT(FrameStr(state.status_text.c_str()),
				          CLAY_TEXT_CONFIG(::ui::kFontHeading));
			}
		}
		if(!state.progress_text.empty()){
			CLAY({ .id = CLAY_ID("UpdateProgress") }) {
				CLAY_TEXT(FrameStr(state.progress_text.c_str()),
				          CLAY_TEXT_CONFIG(::ui::kFontHeading));
			}
		}
		switch(state.left){
			case UpdateState::LeftButton::Update:
				MenuButton("update", "Update", h.on_update);
			break;
			case UpdateState::LeftButton::Retry:
				MenuButton("retry", "Retry", h.on_retry);
			break;
			case UpdateState::LeftButton::Download:
				MenuButton("download", "Download", h.on_download);
			break;
			case UpdateState::LeftButton::None: break;
		}
		if(state.show_cancel){
			MenuButton("cancel", "Cancel", h.on_cancel);
		}
	}
}

// -----------------------------------------------------------------------------
// UpdateRuntime — engine wire-in for GameState::UPDATING.
// -----------------------------------------------------------------------------

namespace {

UpdateHandlers BuildUpdateHandlers(ScreenContext & sctx, Updater & updater){
	UpdateHandlers h;
	h.on_update = [&updater](){
		if(updater.GetState() == Updater::PROMPTING){
			updater.Consent();
		}
	};
	h.on_cancel = [&sctx, &updater](){
		Updater::State us = updater.GetState();
		if(us == Updater::PROMPTING || us == Updater::DOWNLOADING || us == Updater::FAILED){
			if(us == Updater::DOWNLOADING){
				updater.Cancel();
			}
			sctx.GoToState(GameState::MAINMENU);
		}
	};
	h.on_retry = [&updater](){
		if(updater.GetState() == Updater::FAILED && updater.GetRetryCount() < 3){
			updater.Retry();
		}
	};
	h.on_download = [&sctx, &updater](){
		if(updater.GetState() == Updater::FAILED && updater.GetRetryCount() >= 3){
			std::string url = updater.GetDownloadURL();
#ifdef _WIN32
			std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
			std::string cmd = "open '" + url + "'";
#else
			std::string cmd = "xdg-open '" + url + "' &";
#endif
			system(cmd.c_str());
			sctx.GoToState(GameState::MAINMENU);
		}
	};
	return h;
}

UpdateState CurrentUpdate(Updater & updater){
	UpdateState s;
	Updater::State us = updater.GetState();
	switch(us){
		case Updater::PROMPTING:
			s.left = UpdateState::LeftButton::Update;
			s.show_cancel = true;
			s.status_text = "An update is required to play online.";
		break;
		case Updater::DOWNLOADING:{
			s.left = UpdateState::LeftButton::None;
			s.show_cancel = true;
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%d%%", int(updater.GetProgress() * 100));
			s.status_text = buf;
			int width = int(updater.GetProgress() * 20.0f);
			std::string bar = "[";
			for(int i = 0; i < 20; i++){
				bar += (i < width) ? "=" : " ";
			}
			bar += "]";
			s.progress_text = bar;
		}break;
		case Updater::VERIFYING:
			s.left = UpdateState::LeftButton::None;
			s.show_cancel = true;
			s.status_text = "Verifying...";
		break;
		case Updater::STAGING:
			s.left = UpdateState::LeftButton::None;
			s.show_cancel = true;
			s.status_text = "Restarting...";
		break;
		case Updater::FAILED:{
			s.show_cancel = true;
			s.status_text = updater.GetErrorMessage();
			s.left = (updater.GetRetryCount() < 3)
				? UpdateState::LeftButton::Retry
				: UpdateState::LeftButton::Download;
		}break;
		case Updater::IDLE:
		case Updater::DONE:
		default:
			s.left = UpdateState::LeftButton::None;
			s.show_cancel = true;
		break;
	}
	return s;
}

}  // namespace

UpdateRuntime::UpdateRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

void UpdateRuntime::Render(Surface & target, ::Renderer & renderer,
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
	UpdateHandlers handlers = BuildUpdateHandlers(sctx_, sctx_.updater);
	UpdateState live = CurrentUpdate(sctx_.updater);
	RenderUpdate(ctx, handlers, live);
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	::ui::DrawRenderCommands(cmds, renderer, target, scale);
}

bool UpdateRuntime::DispatchMouseDown(int mouse_x, int mouse_y,
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

	// Re-lay out so OnHover userData points at `handlers` on this stack frame;
	// the prior Render frame's tree captured a now-destroyed handlers struct.
	EnsureClayContext(ctx);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	UpdateHandlers handlers = BuildUpdateHandlers(sctx_, sctx_.updater);
	UpdateState live = CurrentUpdate(sctx_.updater);
	RenderUpdate(ctx, handlers, live);
	(void)Clay_EndLayout();
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/true);
	return true;
}

void UpdateRuntime::Tick(){
	// Mirror UpdateScreen::Tick's STAGING branch: on STAGING, spawn the
	// stage-2 child; on success flag the Updater so Game::Loop returns
	// false next tick and ~Game tears down SDL/audio cleanly before the
	// new process opens the device (avoids audible pop).
	Updater & updater = sctx_.updater;
	if(updater.GetState() != Updater::STAGING) return;
	std::string zippath =
#ifdef _WIN32
		std::string(std::getenv("TEMP") ? std::getenv("TEMP") : ".") + "\\silencer-update.zip";
#else
		"/tmp/silencer-update.zip";
#endif
	std::fprintf(stderr, "[updater] UpdateRuntime::Tick invoking UpdaterStage2::Launch with zip=%s\n",
		zippath.c_str());
	if(UpdaterStage2::Launch(zippath)){
		updater.MarkStage2Spawned();
		return;
	}
	std::fprintf(stderr, "[updater] UpdaterStage2::Launch failed; returning to main menu\n");
	sctx_.GoToState(GameState::MAINMENU);
}

}  // namespace v2
}  // namespace ui
