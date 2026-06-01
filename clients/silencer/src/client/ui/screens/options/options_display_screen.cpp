#include "options_display_screen.h"

#include "client/ui/hooks/use_navigation.h"
#include "client/ui/hooks/use_options.h"
#include "client/ui/screens/options/options_display_frame.h"
#include "screen_context.h"

#include "runtime/UiInteractionRegistry.h"

#include <algorithm>

void OptionsDisplayScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.CenterPresentationCamera();
}

void OptionsDisplayScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

void OptionsDisplayScreen::BuildUi(ScreenContext & ctx, float frametime, const silencer::ui::UiInputState& input, Uint8, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;

	silencer::client_ui::OptionsModel options =
		silencer::client_ui::use_options(
			silencer::client_ui::MakeOptionsProvider(ctx));
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();
	const float uiScale = input.uiScale;
	const int virtualW = std::max(1, input.width);
	const int virtualH = std::max(1, input.height);
	silencer::client_ui::OptionsDisplayFrameProps props{
		.key = "options-display",
		.fullscreen_enabled = options.display.fullscreen_enabled(),
		.smooth_scaling_enabled = options.display.smooth_scaling_enabled(),
		.toggle_fullscreen = [options]() {
			options.display.toggle_fullscreen();
		},
		.toggle_smooth_scaling = [options]() {
			options.display.toggle_smooth_scaling();
		},
		.save = [options, navigation]() {
			options.display.save();
			navigation.pop_top();
		},
		.cancel = [options, navigation]() {
			options.display.cancel();
			navigation.pop_top();
		},
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
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		silencer::client_ui::OptionsModel options =
			silencer::client_ui::use_options(
				silencer::client_ui::MakeOptionsProvider(ctx));
		options.display.cancel();
		silencer::client_ui::use_navigation().pop_top();
		return true;
	}
	return retainedFrame_.HandleUiIntent(action);
}

const ::ui::DrawCommandList * OptionsDisplayScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
