#include "options_display_screen.h"

#include "client/ui/screens/options/options_display_view.h"
#include "screen_context.h"
#include "renderer.h"
#include "config.h"
#include "renderdevice.h"
#include "world.h"

#include <SDL3/SDL_video.h>

namespace options_display_screen_detail
{
void ToggleFullscreen(SDL_Window * window)
{
	Config & cfg = Config::GetInstance();
	cfg.fullscreen = !cfg.fullscreen;
	if(window) SDL_SetWindowFullscreen(window, cfg.fullscreen);
}

void ToggleSmoothScaling(RenderDevice * renderdevice)
{
	Config & cfg = Config::GetInstance();
	cfg.scalefilter = !cfg.scalefilter;
	if(renderdevice) renderdevice->SetScaleFilter(cfg.scalefilter);
}

void SaveDisplaySettings()
{
	Config::GetInstance().Save();
}

void CancelDisplaySettings(SDL_Window * window, RenderDevice * renderdevice)
{
	Config & cfg = Config::GetInstance();
	cfg.Load();
	if(renderdevice) renderdevice->SetScaleFilter(cfg.scalefilter);
	if(window) SDL_SetWindowFullscreen(window, cfg.fullscreen);
}
} // namespace options_display_screen_detail

void OptionsDisplayScreen::Build(ScreenContext & ctx)
{
	ctx.world.DestroyAllObjects();
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
}

void OptionsDisplayScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsDisplayScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;

	Config & cfg = Config::GetInstance();
	const silencer::client_ui::OptionsDisplay display{
		.fullscreen = cfg.fullscreen,
		.smooth_scaling = cfg.scalefilter,
		.toggle_fullscreen = [window = ctx.window]() {
			options_display_screen_detail::ToggleFullscreen(window);
		},
		.toggle_smooth_scaling = [renderdevice = ctx.renderdevice]() {
			options_display_screen_detail::ToggleSmoothScaling(renderdevice);
		},
		.save = []() {
			options_display_screen_detail::SaveDisplaySettings();
		},
		.cancel = [window = ctx.window, renderdevice = ctx.renderdevice]() {
			options_display_screen_detail::CancelDisplaySettings(window, renderdevice);
		},
	};
	const auto * stored = ::ui::copy_value(display);
	if(!stored) return false;
	*out = ::ui::component(
		"OptionsDisplayView",
		silencer::client_ui::OptionsDisplayViewProps{
			.key = "options-display",
			.display = stored,
		},
		silencer::client_ui::OptionsDisplayView);
	return true;
}

void OptionsDisplayScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsDisplayScreen::HandleBack(ScreenContext & ctx)
{
	options_display_screen_detail::CancelDisplaySettings(ctx.window, ctx.renderdevice);
	ctx.PopScreen();
	return true;
}
