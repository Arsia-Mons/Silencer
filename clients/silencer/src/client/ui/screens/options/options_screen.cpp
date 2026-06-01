#include "options_screen.h"

#include "client/ui/hooks/use_navigation.h"
#include "main_menu_screen.h"
#include "options_audio_screen.h"
#include "options_controls_screen.h"
#include "options_display_screen.h"
#include "client/ui/screens/options/options_frame.h"
#include "screen_context.h"
#include "clay_ui_compositor.h"
#include "renderer.h"
#include "surface.h"

#include "runtime/UiInteractionRegistry.h"

#include <algorithm>
#include <memory>

namespace options_screen_detail {

constexpr const char * kActionControls = "options.controls";
constexpr const char * kActionDisplay = "options.display";
constexpr const char * kActionAudio = "options.audio";
constexpr const char * kActionBack = "options.back";

}  // namespace options_screen_detail

void OptionsScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	goBackClicked = false;
	controlsClicked = false;
	displayClicked = false;
	audioClicked = false;
}

void OptionsScreen::Tick(ScreenContext & ctx)
{
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();
	if(goBackClicked){
		goBackClicked = false;
		navigation.pop_top();
		return;
	}
	if(controlsClicked){
		controlsClicked = false;
		navigation.push(std::make_unique<OptionsControlsScreen>());
		return;
	}
	if(displayClicked){
		displayClicked = false;
		navigation.push(std::make_unique<OptionsDisplayScreen>());
		return;
	}
	if(audioClicked){
		audioClicked = false;
		navigation.push(std::make_unique<OptionsAudioScreen>());
		return;
	}
}

void OptionsScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)ctx;
	(void)frametime;
	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));
	silencer::client_ui::OptionsFrameProps props{
		.key = "options-screen",
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::OptionsFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);
}

void OptionsScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		goBackClicked = true;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_screen_detail::kActionControls){
		controlsClicked = true;
		return true;
	}
	if(action.id == options_screen_detail::kActionDisplay){
		displayClicked = true;
		return true;
	}
	if(action.id == options_screen_detail::kActionAudio){
		audioClicked = true;
		return true;
	}
	if(action.id == options_screen_detail::kActionBack){
		goBackClicked = true;
		return true;
	}
	(void)ctx;
	return false;
}

const ::ui::DrawCommandList * OptionsScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
