#include "options_screen.h"

#include "client/ui/hooks/use_navigation.h"
#include "main_menu_screen.h"
#include "options_audio_screen.h"
#include "options_controls_screen.h"
#include "options_display_screen.h"
#include "client/ui/screens/options/options_frame.h"
#include "screen_context.h"
#include "renderer.h"

#include "runtime/UiInteractionRegistry.h"

#include <algorithm>
#include <memory>

void OptionsScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
}

void OptionsScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

void OptionsScreen::BuildUi(ScreenContext & ctx, float frametime, const silencer::ui::UiInputState& input, Uint8, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)ctx;
	(void)frametime;
	const float uiScale = input.uiScale;
	const int virtualW = std::max(1, input.width);
	const int virtualH = std::max(1, input.height);
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();
	silencer::client_ui::OptionsFrameProps props{
		.key = "options-screen",
		.open_controls = [navigation]() {
			navigation.push(std::make_unique<OptionsControlsScreen>());
		},
		.open_display = [navigation]() {
			navigation.push(std::make_unique<OptionsDisplayScreen>());
		},
		.open_audio = [navigation]() {
			navigation.push(std::make_unique<OptionsAudioScreen>());
		},
		.go_back = [navigation]() {
			navigation.pop_top();
		},
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
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		silencer::client_ui::use_navigation().pop_top();
		return true;
	}
	return retainedFrame_.HandleUiIntent(action);
}

const ::ui::DrawCommandList * OptionsScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
