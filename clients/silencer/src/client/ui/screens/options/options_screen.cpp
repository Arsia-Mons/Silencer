#include "options_screen.h"

#include "client/ui/ClientUi.h"
#include "screen_context.h"
#include "game_state.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"

namespace options_screen_detail {

using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

// Legacy options buttons used a 52px vertical pitch; Oval/Md buttons are 33px tall.
constexpr uint16_t kButtonGap = 19;
constexpr const char * kActionControls = "options.controls";
constexpr const char * kActionDisplay = "options.display";
constexpr const char * kActionAudio = "options.audio";
constexpr const char * kActionBack = "options.back";

std::function<void()> UseQueuedStateTransition(ScreenContext & ctx, Uint8 state)
{
	auto queueWrite = silencer::client_ui::UseUiWriteQueue();
	if(!queueWrite) return {};
	return [queueWrite, &ctx, state]() {
		queueWrite([&ctx, state]() {
			ctx.GoToState(state);
		});
	};
}

void Invoke(const std::function<void()> & action)
{
	if(action) action();
}

}  // namespace options_screen_detail

void OptionsScreen::Build(ScreenContext & ctx)
{
	ctx.ResetMenuPresentation(1);

	// Clay owns all visible options-menu structure and hit targets.

	goBack = {};
	openControls = {};
	openDisplay = {};
	openAudio = {};
}

void OptionsScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

void OptionsScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using namespace silencer::clay_bridge;

	openControls = options_screen_detail::UseQueuedStateTransition(
		ctx, GameState::OPTIONSCONTROLS);
	openDisplay = options_screen_detail::UseQueuedStateTransition(
		ctx, GameState::OPTIONSDISPLAY);
	openAudio = options_screen_detail::UseQueuedStateTransition(
		ctx, GameState::OPTIONSAUDIO);
	goBack = options_screen_detail::UseQueuedStateTransition(
		ctx, GameState::MAINMENU);

	CLAY({ .id = CLAY_ID("OptionsRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsButtonColumn"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(196),
		                       CLAY_SIZING_FIT(0) },
		           .childGap = options_screen_detail::kButtonGap,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			options_screen_detail::Button(CLAY_STRING("OptionsControlsButton"), CLAY_STRING("Controls"),
			           options_screen_detail::ButtonOpts{ .variant = options_screen_detail::ButtonVariant::Oval,
			                                             .size = options_screen_detail::ButtonSize::Md },
			           options_screen_detail::ButtonHandle{ nullptr, options_screen_detail::kActionControls, &interactions });
			options_screen_detail::Button(CLAY_STRING("OptionsDisplayButton"), CLAY_STRING("Display"),
			           options_screen_detail::ButtonOpts{ .variant = options_screen_detail::ButtonVariant::Oval,
			                                             .size = options_screen_detail::ButtonSize::Md },
			           options_screen_detail::ButtonHandle{ nullptr, options_screen_detail::kActionDisplay, &interactions });
			options_screen_detail::Button(CLAY_STRING("OptionsAudioButton"), CLAY_STRING("Audio"),
			           options_screen_detail::ButtonOpts{ .variant = options_screen_detail::ButtonVariant::Oval,
			                                             .size = options_screen_detail::ButtonSize::Md },
			           options_screen_detail::ButtonHandle{ nullptr, options_screen_detail::kActionAudio, &interactions });
			options_screen_detail::Button(CLAY_STRING("OptionsBackButton"), CLAY_STRING("Go Back"),
			           options_screen_detail::ButtonOpts{ .variant = options_screen_detail::ButtonVariant::Oval,
			                                             .size = options_screen_detail::ButtonSize::Md },
			           options_screen_detail::ButtonHandle{ nullptr, options_screen_detail::kActionBack, &interactions });
		}
	}
}

void OptionsScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
	goBack = {};
	openControls = {};
	openDisplay = {};
	openAudio = {};
}

bool OptionsScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		(void)ctx;
		options_screen_detail::Invoke(goBack);
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_screen_detail::kActionControls){
		(void)ctx;
		options_screen_detail::Invoke(openControls);
		return true;
	}
	if(action.id == options_screen_detail::kActionDisplay){
		(void)ctx;
		options_screen_detail::Invoke(openDisplay);
		return true;
	}
	if(action.id == options_screen_detail::kActionAudio){
		(void)ctx;
		options_screen_detail::Invoke(openAudio);
		return true;
	}
	if(action.id == options_screen_detail::kActionBack){
		(void)ctx;
		options_screen_detail::Invoke(goBack);
		return true;
	}
	(void)ctx;
	return false;
}
