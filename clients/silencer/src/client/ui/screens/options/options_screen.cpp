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

namespace {

using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;

constexpr uint16_t kButtonGap = 12;
constexpr const char * kActionControls = "options.controls";
constexpr const char * kActionDisplay = "options.display";
constexpr const char * kActionAudio = "options.audio";
constexpr const char * kActionBack = "options.back";

void RegisterButton(const char * label,
                    const char * actionId,
                    int x,
                    int y,
                    silencer::ui::UiInteractionRegistry& interactions)
{
	silencer::ui::UiInteractable w;
	w.id = actionId;
	w.labelText = label;
	w.kind = silencer::ui::UiInteractableKind::Button;
	w.x = x; w.y = y; w.w = 156; w.h = 21;
	interactions.RegisterInteractable(w);
}

void RegisterOptionsButtons(silencer::ui::UiInteractionRegistry& interactions)
{
	RegisterButton("Controls", kActionControls, 242, 160, interactions);
	RegisterButton("Display", kActionDisplay, 242, 193, interactions);
	RegisterButton("Audio", kActionAudio, 242, 226, interactions);
	RegisterButton("Go Back", kActionBack, 242, 259, interactions);
}

}  // namespace

void OptionsScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	// Clay owns all visible options-menu structure and hit targets.

	goBackClicked = false;
	controlsClicked = false;
	displayClicked = false;
	audioClicked = false;

	RegisterOptionsButtons(ctx.game.UiInteractions());
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
	using namespace silencer::clay_bridge;



	CLAY({ .id = CLAY_ID("OptionsRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)dst.w),
	                       CLAY_SIZING_FIXED((float)dst.h) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsButtonColumn"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(156),
		                       CLAY_SIZING_FIT(0) },
		           .childGap = kButtonGap,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			BankButton(CLAY_STRING("Controls"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, kActionControls, &interactions });
			BankButton(CLAY_STRING("Display"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, kActionDisplay, &interactions });
			BankButton(CLAY_STRING("Audio"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, kActionAudio, &interactions });
			BankButton(CLAY_STRING("Go Back"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, kActionBack, &interactions });
		}
	}

	RegisterOptionsButtons(interactions);
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
	if(action.id == kActionControls){
		controlsClicked = true;
		return true;
	}
	if(action.id == kActionDisplay){
		displayClicked = true;
		return true;
	}
	if(action.id == kActionAudio){
		audioClicked = true;
		return true;
	}
	if(action.id == kActionBack){
		goBackClicked = true;
		return true;
	}
	(void)ctx;
	return false;
}
