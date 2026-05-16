#include "options_display_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"
#include "config.h"
#include "renderdevice.h"

#include "components/boolean_setting_row.h"
#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

namespace options_display_screen_detail
{
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;

constexpr uint16_t kPanelW = 420;
constexpr uint16_t kPanelPadX = 24;
constexpr uint16_t kPanelPadY = 32;
constexpr uint16_t kRowGap = 20;
constexpr uint16_t kActionGap = 12;
constexpr const char * kActionFullscreen = "options_display.fullscreen";
constexpr const char * kActionSmoothScaling = "options_display.smooth_scaling";
constexpr const char * kActionSave = "options_display.save";
constexpr const char * kActionCancel = "options_display.cancel";
} // namespace options_display_screen_detail

void OptionsDisplayScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	fullscreenClicked = false;
	smoothScalingClicked = false;
	saveClicked = false;
	cancelClicked = false;
}

void OptionsDisplayScreen::Tick(ScreenContext & ctx)
{
	if(fullscreenClicked){
		fullscreenClicked = false;
		Config & cfg = Config::GetInstance();
		cfg.fullscreen = !cfg.fullscreen;
		if(ctx.window) SDL_SetWindowFullscreen(ctx.window, cfg.fullscreen);
	}
	if(smoothScalingClicked){
		smoothScalingClicked = false;
		Config & cfg = Config::GetInstance();
		cfg.scalefilter = !cfg.scalefilter;
		if(ctx.renderdevice) ctx.renderdevice->SetScaleFilter(cfg.scalefilter);
	}
	if(saveClicked){
		saveClicked = false;
		Config::GetInstance().Save();
		ctx.GoToState(GameState::OPTIONS);
		return;
	}
	if(cancelClicked){
		cancelClicked = false;
		Config & cfg = Config::GetInstance();
		cfg.Load();
		if(ctx.renderdevice) ctx.renderdevice->SetScaleFilter(cfg.scalefilter);
		if(ctx.window) SDL_SetWindowFullscreen(ctx.window, cfg.fullscreen);
		ctx.GoToState(GameState::OPTIONS);
	}
}

void OptionsDisplayScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using namespace silencer::clay_bridge;

	Config & cfg = Config::GetInstance();
	CLAY({ .id = CLAY_ID("OptionsDisplayRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .padding = { 0, 0, 80, 0 },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsDisplayPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(options_display_screen_detail::kPanelW),
		                       CLAY_SIZING_FIT(0) },
		           .padding = { options_display_screen_detail::kPanelPadX, options_display_screen_detail::kPanelPadX,
		                        options_display_screen_detail::kPanelPadY, options_display_screen_detail::kPanelPadY },
		           .childGap = 22,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			options_display_screen_detail::BankText(CLAY_STRING("Display Options"), options_display_screen_detail::BankTextVariant::Title, {});
			CLAY({ .id = CLAY_ID("OptionsDisplayRows"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			           .childGap = options_display_screen_detail::kRowGap,
			           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				silencer::client_ui::options::BooleanSettingRow(
					CLAY_STRING("OptionsDisplayFullscreenRow"),
					CLAY_STRING("OptionsDisplayFullscreenButton"),
					CLAY_STRING("Fullscreen"),
					cfg.fullscreen,
					options_display_screen_detail::kActionFullscreen,
					interactions);
				silencer::client_ui::options::BooleanSettingRow(
					CLAY_STRING("OptionsDisplaySmoothScalingRow"),
					CLAY_STRING("OptionsDisplaySmoothScalingButton"),
					CLAY_STRING("Smooth Scaling"),
					cfg.scalefilter,
					options_display_screen_detail::kActionSmoothScaling,
					interactions);
			}
			CLAY({ .id = CLAY_ID("OptionsDisplayActions"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = options_display_screen_detail::kActionGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				options_display_screen_detail::Button(CLAY_STRING("OptionsDisplaySaveButton"), CLAY_STRING("Save"),
				           options_display_screen_detail::ButtonOpts{ .variant = options_display_screen_detail::ButtonVariant::Oval,
				                                                      .size = options_display_screen_detail::ButtonSize::Md },
				           options_display_screen_detail::ButtonHandle{ nullptr, options_display_screen_detail::kActionSave, &interactions });
				options_display_screen_detail::Button(CLAY_STRING("OptionsDisplayCancelButton"), CLAY_STRING("Cancel"),
				           options_display_screen_detail::ButtonOpts{ .variant = options_display_screen_detail::ButtonVariant::Oval,
				                                                      .size = options_display_screen_detail::ButtonSize::Md },
				           options_display_screen_detail::ButtonHandle{ nullptr, options_display_screen_detail::kActionCancel, &interactions });
			}
		}
	}
}

void OptionsDisplayScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsDisplayScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		cancelClicked = true;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_display_screen_detail::kActionFullscreen){
		fullscreenClicked = true;
		return true;
	}
	if(action.id == options_display_screen_detail::kActionSmoothScaling){
		smoothScalingClicked = true;
		return true;
	}
	if(action.id == options_display_screen_detail::kActionSave){
		saveClicked = true;
		return true;
	}
	if(action.id == options_display_screen_detail::kActionCancel){
		cancelClicked = true;
		return true;
	}
	return false;
}
