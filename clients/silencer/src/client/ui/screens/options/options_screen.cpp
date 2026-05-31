#include "options_screen.h"

#include "client/ui/screens/options/options_menu_view.h"
#include "screen_context.h"
#include "game_state.h"
#include "renderer.h"

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
	if(goBackClicked){
		goBackClicked = false;
		ctx.GoToState(GameState::MAINMENU);
		return;
	}
	if(controlsClicked){
		controlsClicked = false;
		ctx.GoToState(GameState::OPTIONSCONTROLS);
		return;
	}
	if(displayClicked){
		displayClicked = false;
		ctx.GoToState(GameState::OPTIONSDISPLAY);
		return;
	}
	if(audioClicked){
		audioClicked = false;
		ctx.GoToState(GameState::OPTIONSAUDIO);
		return;
	}
}

bool OptionsScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	(void)ctx;
	if(!out) return false;
	*out = silencer::client_ui::OptionsMenuView(
		silencer::client_ui::OptionsMenuViewProps{
			.key = "options-menu",
			.on_controls = [this](const ::ui::ActivationEvent&) {
				controlsClicked = true;
			},
			.on_display = [this](const ::ui::ActivationEvent&) {
				displayClicked = true;
			},
			.on_audio = [this](const ::ui::ActivationEvent&) {
				audioClicked = true;
			},
			.on_back = [this](const ::ui::ActivationEvent&) {
				goBackClicked = true;
			},
		});
	return true;
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
