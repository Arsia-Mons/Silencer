#include "options_display_screen.h"

#include "client/ui/screens/options/options_display_view.h"
#include "screen_context.h"
#include "game_state.h"
#include "renderer.h"
#include "config.h"
#include "renderdevice.h"

#include <SDL3/SDL_video.h>

namespace options_display_screen_detail
{
constexpr const char * kActionFullscreen = "options_display.fullscreen";
constexpr const char * kActionSmoothScaling = "options_display.smooth_scaling";
constexpr const char * kActionSave = "options_display.save";
constexpr const char * kActionCancel = "options_display.cancel";
} // namespace options_display_screen_detail

void OptionsDisplayScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	fullscreenClicked = false;
	smoothScalingClicked = false;
	saveClicked = false;
	cancelClicked = false;
}

void OptionsDisplayScreen::Tick(ScreenContext & ctx)
{
	if(fullscreenClicked){
		fullscreenClicked = false;
		Config & cfg = Config::GetInstance();
		cfg.fullscreen = !cfg.fullscreen;
		if(ctx.window) SDL_SetWindowFullscreen(ctx.window, cfg.fullscreen);
	}
	if(smoothScalingClicked){
		smoothScalingClicked = false;
		Config & cfg = Config::GetInstance();
		cfg.scalefilter = !cfg.scalefilter;
		if(ctx.renderdevice) ctx.renderdevice->SetScaleFilter(cfg.scalefilter);
	}
	if(saveClicked){
		saveClicked = false;
		Config::GetInstance().Save();
		ctx.GoToState(GameState::OPTIONS);
		return;
	}
	if(cancelClicked){
		cancelClicked = false;
		Config & cfg = Config::GetInstance();
		cfg.Load();
		if(ctx.renderdevice) ctx.renderdevice->SetScaleFilter(cfg.scalefilter);
		if(ctx.window) SDL_SetWindowFullscreen(ctx.window, cfg.fullscreen);
		ctx.GoToState(GameState::OPTIONS);
	}
}

bool OptionsDisplayScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	(void)ctx;
	if(!out) return false;

	Config & cfg = Config::GetInstance();
	*out = silencer::client_ui::OptionsDisplayView(
		silencer::client_ui::OptionsDisplayViewProps{
			.key = "options-display",
			.fullscreen = cfg.fullscreen,
			.smooth_scaling = cfg.scalefilter,
			.on_fullscreen = [this](bool) {
				fullscreenClicked = true;
			},
			.on_smooth_scaling = [this](bool) {
				smoothScalingClicked = true;
			},
			.on_save = [this](const ::ui::ActivationEvent&) {
				saveClicked = true;
			},
			.on_cancel = [this](const ::ui::ActivationEvent&) {
				cancelClicked = true;
			},
		});
	return true;
}

void OptionsDisplayScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsDisplayScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		cancelClicked = true;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_display_screen_detail::kActionFullscreen){
		fullscreenClicked = true;
		return true;
	}
	if(action.id == options_display_screen_detail::kActionSmoothScaling){
		smoothScalingClicked = true;
		return true;
	}
	if(action.id == options_display_screen_detail::kActionSave){
		saveClicked = true;
		return true;
	}
	if(action.id == options_display_screen_detail::kActionCancel){
		cancelClicked = true;
		return true;
	}
	return false;
}
