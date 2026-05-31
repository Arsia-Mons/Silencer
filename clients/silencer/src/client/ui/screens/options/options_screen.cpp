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
}

void OptionsScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	(void)ctx;
	if(!out) return false;
	*out = ::ui::component(
		"OptionsMenuView",
		silencer::client_ui::OptionsMenuViewProps{
			.key = "options-menu",
		},
		silencer::client_ui::OptionsMenuView);
	return true;
}

void OptionsScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		ctx.GoToState(GameState::MAINMENU);
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_screen_detail::kActionControls){
		ctx.GoToState(GameState::OPTIONSCONTROLS);
		return true;
	}
	if(action.id == options_screen_detail::kActionDisplay){
		ctx.GoToState(GameState::OPTIONSDISPLAY);
		return true;
	}
	if(action.id == options_screen_detail::kActionAudio){
		ctx.GoToState(GameState::OPTIONSAUDIO);
		return true;
	}
	if(action.id == options_screen_detail::kActionBack){
		ctx.GoToState(GameState::MAINMENU);
		return true;
	}
	return false;
}
