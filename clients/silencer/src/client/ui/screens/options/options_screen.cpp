#include "options_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "world.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "clay_inspector.h"
#include "primitives/bank_button.h"

#include <SDL3/SDL.h>

namespace {

using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonBeginFrame;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;

constexpr uint16_t kButtonGap = 12;

void OnGoBackClicked(void * user)
{
	auto * screen = static_cast<OptionsScreen *>(user);
	if(screen) screen->NotifyGoBackClicked();
}

void OnControlsClicked(void * user)
{
	auto * screen = static_cast<OptionsScreen *>(user);
	if(screen) screen->NotifyControlsClicked();
}

void OnDisplayClicked(void * user)
{
	auto * screen = static_cast<OptionsScreen *>(user);
	if(screen) screen->NotifyDisplayClicked();
}

void OnAudioClicked(void * user)
{
	auto * screen = static_cast<OptionsScreen *>(user);
	if(screen) screen->NotifyAudioClicked();
}

void RegisterButton(const char * label,
                    int x,
                    int y,
                    void (*onClick)(void *),
                    OptionsScreen * screen)
{
	silencer::ui::clay_inspector::Widget w;
	w.label = label;
	w.kind = silencer::ui::clay_inspector::WidgetKind::Button;
	w.x = x; w.y = y; w.w = 156; w.h = 21;
	w.onClick = onClick;
	w.clickUser = screen;
	silencer::ui::clay_inspector::Register(w);
}

void RegisterOptionsButtons(OptionsScreen * screen)
{
	RegisterButton("Controls", 242, 160, &OnControlsClicked, screen);
	RegisterButton("Display", 242, 193, &OnDisplayClicked, screen);
	RegisterButton("Audio", 242, 226, &OnAudioClicked, screen);
	RegisterButton("Go Back", 242, 259, &OnGoBackClicked, screen);
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

	silencer::ui::clay_inspector::BeginFrame();
	RegisterOptionsButtons(this);
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

void OptionsScreen::Draw(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;

	ctx.BeginClayFrame(dst);

	BankButtonBeginFrame();
	silencer::ui::clay_inspector::BeginFrame();

	Clay_BeginLayout();
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
			           BankButtonHandle{ nullptr, &OnControlsClicked, this });
			BankButton(CLAY_STRING("Display"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, &OnDisplayClicked, this });
			BankButton(CLAY_STRING("Audio"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, &OnAudioClicked, this });
			BankButton(CLAY_STRING("Go Back"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, &OnGoBackClicked, this });
		}
	}
	Clay_RenderCommandArray cmds = Clay_EndLayout();

	Render(ctx.game, &dst, cmds);
	RegisterOptionsButtons(this);
}

void OptionsScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
