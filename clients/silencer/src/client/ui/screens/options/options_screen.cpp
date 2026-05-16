#include "options_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "world.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_button.h"

#include <SDL3/SDL.h>

namespace options_screen_detail {

using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;

constexpr uint16_t kButtonGap = 12;
constexpr const char * kActionControls = "options.controls";
constexpr const char * kActionDisplay = "options.display";
constexpr const char * kActionAudio = "options.audio";
constexpr const char * kActionBack = "options.back";

}  // namespace options_screen_detail

void OptionsScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	// Clay owns all visible options-menu structure and hit targets.

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

void OptionsScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using namespace silencer::clay_bridge;

	CLAY({ .id = CLAY_ID("OptionsRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsButtonColumn"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(156),
		                       CLAY_SIZING_FIT(0) },
		           .childGap = options_screen_detail::kButtonGap,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			options_screen_detail::BankButton(CLAY_STRING("Controls"), options_screen_detail::BankButtonVariant::Chrome, {},
			           options_screen_detail::BankButtonHandle{ nullptr, options_screen_detail::kActionControls, &interactions });
			options_screen_detail::BankButton(CLAY_STRING("Display"), options_screen_detail::BankButtonVariant::Chrome, {},
			           options_screen_detail::BankButtonHandle{ nullptr, options_screen_detail::kActionDisplay, &interactions });
			options_screen_detail::BankButton(CLAY_STRING("Audio"), options_screen_detail::BankButtonVariant::Chrome, {},
			           options_screen_detail::BankButtonHandle{ nullptr, options_screen_detail::kActionAudio, &interactions });
			options_screen_detail::BankButton(CLAY_STRING("Go Back"), options_screen_detail::BankButtonVariant::Chrome, {},
			           options_screen_detail::BankButtonHandle{ nullptr, options_screen_detail::kActionBack, &interactions });
		}
	}
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
