#include "options_display_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"
#include "config.h"
#include "renderdevice.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

namespace
{
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonBeginFrame;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextBeginFrame;
using silencer::ui::primitives::BankTextVariant;

constexpr uint16_t kPanelW = 420;
constexpr uint16_t kPanelPadX = 24;
constexpr uint16_t kPanelPadY = 32;
constexpr uint16_t kRowH = 33;
constexpr uint16_t kRowGap = 20;
constexpr uint16_t kIndicatorGap = 10;
constexpr uint16_t kActionGap = 12;
constexpr const char * kActionFullscreen = "options_display.fullscreen";
constexpr const char * kActionSmoothScaling = "options_display.smooth_scaling";
constexpr const char * kActionSave = "options_display.save";
constexpr const char * kActionCancel = "options_display.cancel";

void RegisterButton(const char * label,
                    const char * actionId,
                    int x,
                    int y)
{
	silencer::ui::automation::Widget w;
	w.id = actionId;
	w.labelText = label;
	w.kind = silencer::ui::automation::WidgetKind::Button;
	w.x = x; w.y = y; w.w = 156; w.h = 21;
	silencer::ui::automation::Register(w);
}

void RegisterWidgets(int surfaceW, int surfaceH)
{
	const int panelX = (surfaceW - kPanelW) / 2;
	const int panelY = 80;
	const int rowX = panelX + kPanelPadX;
	const int rowY = panelY + kPanelPadY + 42;
	const int actionY = rowY + (kRowH + kRowGap) * 2 + 8;
	RegisterButton("Fullscreen", kActionFullscreen, rowX, rowY + 6);
	RegisterButton("Smooth Scaling", kActionSmoothScaling,
	               rowX, rowY + kRowH + kRowGap + 6);
	RegisterButton("Save", kActionSave, panelX + 48, actionY);
	RegisterButton("Cancel", kActionCancel, panelX + 216, actionY);
}

void ToggleIndicator(Clay_String id, bool selected)
{
	CLAY({ .id = CLAY_SIDI(id, 2),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(16) },
	           .childGap = kIndicatorGap,
	           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		CLAY({ .id = CLAY_SIDI(id, 3),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(20), CLAY_SIZING_FIXED(16) },
		       },
		       .image = { .imageData = silencer::clay_bridge::PackImage(
		                     6, selected ? 12 : 13) } }) {}
		CLAY({ .id = CLAY_SIDI(id, 4),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(20), CLAY_SIZING_FIXED(16) },
		       },
		       .image = { .imageData = silencer::clay_bridge::PackImage(
		                     6, selected ? 15 : 14) } }) {}
	}
}

void ToggleRow(Clay_String label,
               bool selected,
               const char * actionId)
{
	CLAY({ .id = CLAY_SID(label),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kRowH) },
	           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		BankButton(label, BankButtonVariant::Chrome, {},
		           BankButtonHandle{ nullptr, actionId });
		CLAY({ .id = CLAY_SIDI(label, 1),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
		           .childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER },
		       } }) {
			ToggleIndicator(label, selected);
		}
	}
}
}

void OptionsDisplayScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	fullscreenClicked = false;
	smoothScalingClicked = false;
	saveClicked = false;
	cancelClicked = false;
	const Surface& surface = ctx.game.GetScreenBuffer();
	RegisterWidgets(surface.w, surface.h);
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

void OptionsDisplayScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;



	Config & cfg = Config::GetInstance();
	CLAY({ .id = CLAY_ID("OptionsDisplayRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)dst.w),
	                       CLAY_SIZING_FIXED((float)dst.h) },
	           .padding = { 0, 0, 80, 0 },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsDisplayPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kPanelW),
		                       CLAY_SIZING_FIT(0) },
		           .padding = { kPanelPadX, kPanelPadX,
		                        kPanelPadY, kPanelPadY },
		           .childGap = 22,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			BankText(CLAY_STRING("Display Options"), BankTextVariant::Title, {});
			CLAY({ .id = CLAY_ID("OptionsDisplayRows"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			           .childGap = kRowGap,
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				ToggleRow(CLAY_STRING("Fullscreen"), cfg.fullscreen,
				          kActionFullscreen);
				ToggleRow(CLAY_STRING("Smooth Scaling"), cfg.scalefilter,
				          kActionSmoothScaling);
			}
			CLAY({ .id = CLAY_ID("OptionsDisplayActions"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = kActionGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				BankButton(CLAY_STRING("Save"), BankButtonVariant::Chrome, {},
				           BankButtonHandle{ nullptr, kActionSave });
				BankButton(CLAY_STRING("Cancel"), BankButtonVariant::Chrome, {},
				           BankButtonHandle{ nullptr, kActionCancel });
			}
		}
	}
	RegisterWidgets(dst.w, dst.h);
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
	if(action.id == kActionFullscreen){
		fullscreenClicked = true;
		return true;
	}
	if(action.id == kActionSmoothScaling){
		smoothScalingClicked = true;
		return true;
	}
	if(action.id == kActionSave){
		saveClicked = true;
		return true;
	}
	if(action.id == kActionCancel){
		cancelClicked = true;
		return true;
	}
	return false;
}
