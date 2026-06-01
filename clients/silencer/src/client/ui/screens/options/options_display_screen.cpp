#include "options_display_screen.h"

#include "client/ui/hooks/use_navigation.h"
#include "client/ui/hooks/use_options.h"
#include "client/ui/screens/options/options_display_frame.h"
#include "screen_context.h"
#include "clay_ui_compositor.h"
#include "renderer.h"
#include "surface.h"

#include "runtime/UiInteractionRegistry.h"

#include <algorithm>

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
	silencer::client_ui::OptionsModel options =
		silencer::client_ui::use_options(
			silencer::client_ui::MakeOptionsProvider(ctx));
	if(fullscreenClicked){
		fullscreenClicked = false;
		options.display.toggle_fullscreen();
	}
	if(smoothScalingClicked){
		smoothScalingClicked = false;
		options.display.toggle_smooth_scaling();
	}
	if(saveClicked){
		saveClicked = false;
		options.display.save();
		silencer::client_ui::use_navigation().pop_top();
		return;
	}
	if(cancelClicked){
		cancelClicked = false;
		options.display.cancel();
		silencer::client_ui::use_navigation().pop_top();
	}
}

void OptionsDisplayScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;

	silencer::client_ui::OptionsModel options =
		silencer::client_ui::use_options(
			silencer::client_ui::MakeOptionsProvider(ctx));
	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));
	silencer::client_ui::OptionsDisplayFrameProps props{
		.key = "options-display",
		.fullscreen_enabled = options.display.fullscreen_enabled(),
		.smooth_scaling_enabled = options.display.smooth_scaling_enabled(),
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::OptionsDisplayFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);
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

const ::ui::DrawCommandList * OptionsDisplayScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
