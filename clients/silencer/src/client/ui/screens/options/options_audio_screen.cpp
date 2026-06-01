#include "options_audio_screen.h"

#include "client/ui/hooks/use_navigation.h"
#include "client/ui/hooks/use_options.h"
#include "client/ui/screens/options/options_audio_frame.h"
#include "screen_context.h"
#include "clay_ui_compositor.h"
#include "renderer.h"
#include "surface.h"

#include "runtime/UiInteractionRegistry.h"

#include <algorithm>

namespace options_audio_screen_detail
{
constexpr const char * kActionMusic = "options_audio.music";
constexpr const char * kActionSave = "options_audio.save";
constexpr const char * kActionCancel = "options_audio.cancel";
} // namespace options_audio_screen_detail

void OptionsAudioScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	musicClicked = false;
	saveClicked = false;
	cancelClicked = false;
}

void OptionsAudioScreen::Tick(ScreenContext & ctx)
{
	silencer::client_ui::OptionsModel options =
		silencer::client_ui::use_options(
			silencer::client_ui::MakeOptionsProvider(ctx));
	if(musicClicked){
		musicClicked = false;
		options.audio.toggle_music_enabled();
	}
	if(saveClicked){
		saveClicked = false;
		options.audio.save();
		silencer::client_ui::use_navigation().pop_top();
		return;
	}
	if(cancelClicked){
		cancelClicked = false;
		options.audio.cancel();
		silencer::client_ui::use_navigation().pop_top();
	}
}

void OptionsAudioScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;

	silencer::client_ui::OptionsModel options =
		silencer::client_ui::use_options(
			silencer::client_ui::MakeOptionsProvider(ctx));
	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));
	silencer::client_ui::OptionsAudioFrameProps props{
		.key = "options-audio",
		.music_enabled = options.audio.music_enabled(),
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
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		cancelClicked = true;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_audio_screen_detail::kActionMusic){
		musicClicked = true;
		return true;
	}
	if(action.id == options_audio_screen_detail::kActionSave){
		saveClicked = true;
		return true;
	}
	if(action.id == options_audio_screen_detail::kActionCancel){
		cancelClicked = true;
		return true;
	}
	return false;
}

const ::ui::DrawCommandList * OptionsAudioScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
