#include "options_audio_screen.h"

#include "client/ui/hooks/use_navigation.h"
#include "client/ui/hooks/use_options.h"
#include "client/ui/screens/options/options_audio_frame.h"
#include "screen_context.h"
#include "clay_ui_compositor.h"
#include "renderer.h"

#include "runtime/UiInteractionRegistry.h"

#include <algorithm>

void OptionsAudioScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
}

void OptionsAudioScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

void OptionsAudioScreen::BuildUi(ScreenContext & ctx, float frametime, const silencer::ui::UiInputState& input, Uint8, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;

	silencer::client_ui::OptionsModel options =
		silencer::client_ui::use_options(
			silencer::client_ui::MakeOptionsProvider(ctx));
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();
	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, input.width);
	const int virtualH = std::max(1, input.height);
	silencer::client_ui::OptionsAudioFrameProps props{
		.key = "options-audio",
		.music_enabled = options.audio.music_enabled(),
		.toggle_music = [options]() {
			options.audio.toggle_music_enabled();
		},
		.save = [options, navigation]() {
			options.audio.save();
			navigation.pop_top();
		},
		.cancel = [options, navigation]() {
			options.audio.cancel();
			navigation.pop_top();
		},
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::OptionsAudioFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);
}

void OptionsAudioScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsAudioScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		silencer::client_ui::OptionsModel options =
			silencer::client_ui::use_options(
				silencer::client_ui::MakeOptionsProvider(ctx));
		options.audio.cancel();
		silencer::client_ui::use_navigation().pop_top();
		return true;
	}
	return retainedFrame_.HandleUiIntent(action);
}

const ::ui::DrawCommandList * OptionsAudioScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
