#include "options_audio_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"
#include "config.h"
#include "audio.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "clay_inspector.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>

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
constexpr uint16_t kIndicatorGap = 10;
constexpr uint16_t kActionGap = 12;

void ApplyMusicSetting(bool on)
{
	if(on){
		Audio::GetInstance().ResumeMusic();
	}else{
		Audio::GetInstance().PauseMusic();
	}
}

void OnMusicClicked(void * user)
{
	auto * screen = static_cast<OptionsAudioScreen *>(user);
	if(screen) screen->NotifyMusicClicked();
}

void OnSaveClicked(void * user)
{
	auto * screen = static_cast<OptionsAudioScreen *>(user);
	if(screen) screen->NotifySaveClicked();
}

void OnCancelClicked(void * user)
{
	auto * screen = static_cast<OptionsAudioScreen *>(user);
	if(screen) screen->NotifyCancelClicked();
}

void RegisterButton(const char * label,
                    int x,
                    int y,
                    void (*onClick)(void *),
                    OptionsAudioScreen * screen)
{
	silencer::ui::clay_inspector::Widget w;
	w.label = label;
	w.kind = silencer::ui::clay_inspector::WidgetKind::Button;
	w.x = x; w.y = y; w.w = 156; w.h = 21;
	w.onClick = onClick;
	w.clickUser = screen;
	silencer::ui::clay_inspector::Register(w);
}

void RegisterWidgets(OptionsAudioScreen * screen, int surfaceW, int /*surfaceH*/)
{
	const int panelX = (surfaceW - kPanelW) / 2;
	const int panelY = 80;
	const int rowX = panelX + kPanelPadX;
	const int rowY = panelY + kPanelPadY + 42;
	const int actionY = rowY + kRowH + 30;
	RegisterButton("Music", rowX, rowY + 6, &OnMusicClicked, screen);
	RegisterButton("Save", panelX + 48, actionY, &OnSaveClicked, screen);
	RegisterButton("Cancel", panelX + 216, actionY, &OnCancelClicked, screen);
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
               void (*onClick)(void *),
               OptionsAudioScreen * screen)
{
	CLAY({ .id = CLAY_SID(label),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kRowH) },
	           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		BankButton(label, BankButtonVariant::Chrome, {},
		           BankButtonHandle{ nullptr, onClick, screen });
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

void OptionsAudioScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	musicClicked = false;
	saveClicked = false;
	cancelClicked = false;
	silencer::ui::clay_inspector::BeginFrame();
	RegisterWidgets(this, 640, 480);
}

void OptionsAudioScreen::Tick(ScreenContext & ctx)
{
	if(musicClicked){
		musicClicked = false;
		Config & cfg = Config::GetInstance();
		cfg.music = !cfg.music;
		ApplyMusicSetting(cfg.music);
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
		ApplyMusicSetting(cfg.music);
		ctx.GoToState(GameState::OPTIONS);
	}
}

void OptionsAudioScreen::Draw(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;

	EnsureInitialized(dst.w, dst.h);
	float mx = 0.f, my = 0.f;
	Uint32 buttons = SDL_GetMouseState(&mx, &my);
	Clay_SetPointerState({ mx, my },
	                      (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0);

	BankButtonBeginFrame();
	BankTextBeginFrame();
	silencer::ui::clay_inspector::BeginFrame();

	Config & cfg = Config::GetInstance();
	Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("OptionsAudioRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)dst.w),
	                       CLAY_SIZING_FIXED((float)dst.h) },
	           .padding = { 0, 0, 80, 0 },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsAudioPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kPanelW),
		                       CLAY_SIZING_FIT(0) },
		           .padding = { kPanelPadX, kPanelPadX,
		                        kPanelPadY, kPanelPadY },
		           .childGap = 22,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			BankText(CLAY_STRING("Audio Options"), BankTextVariant::Title, {});
			ToggleRow(CLAY_STRING("Music"), cfg.music, &OnMusicClicked, this);
			CLAY({ .id = CLAY_ID("OptionsAudioActions"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = kActionGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				BankButton(CLAY_STRING("Save"), BankButtonVariant::Chrome, {},
				           BankButtonHandle{ nullptr, &OnSaveClicked, this });
				BankButton(CLAY_STRING("Cancel"), BankButtonVariant::Chrome, {},
				           BankButtonHandle{ nullptr, &OnCancelClicked, this });
			}
		}
	}
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	Render(ctx.game, &dst, cmds);
	RegisterWidgets(this, dst.w, dst.h);
}

void OptionsAudioScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
