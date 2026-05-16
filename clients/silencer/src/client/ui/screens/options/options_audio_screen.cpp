#include "options_audio_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"
#include "config.h"
#include "audio.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>

namespace options_audio_screen_detail
{
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;

constexpr uint16_t kPanelW = 420;
constexpr uint16_t kPanelPadX = 24;
constexpr uint16_t kPanelPadY = 32;
constexpr uint16_t kRowH = 33;
constexpr uint16_t kIndicatorGap = 10;
constexpr uint16_t kActionGap = 12;
constexpr const char * kActionMusic = "options_audio.music";
constexpr const char * kActionSave = "options_audio.save";
constexpr const char * kActionCancel = "options_audio.cancel";

void ApplyMusicSetting(bool on)
{
	if(on){
		Audio::GetInstance().ResumeMusic();
	}else{
		Audio::GetInstance().PauseMusic();
	}
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
               const char * actionId,
               silencer::ui::UiInteractionRegistry& interactions)
{
	CLAY({ .id = CLAY_SID(label),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kRowH) },
	           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		BankButton(label, BankButtonVariant::Chrome, {},
		           BankButtonHandle{ nullptr, actionId, &interactions });
		CLAY({ .id = CLAY_SIDI(label, 1),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
		           .childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER },
		       } }) {
			ToggleIndicator(label, selected);
		}
	}
}
} // namespace options_audio_screen_detail

void OptionsAudioScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	musicClicked = false;
	saveClicked = false;
	cancelClicked = false;
}

void OptionsAudioScreen::Tick(ScreenContext & ctx)
{
	if(musicClicked){
		musicClicked = false;
		Config & cfg = Config::GetInstance();
		cfg.music = !cfg.music;
		options_audio_screen_detail::ApplyMusicSetting(cfg.music);
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
		options_audio_screen_detail::ApplyMusicSetting(cfg.music);
		ctx.GoToState(GameState::OPTIONS);
	}
}

void OptionsAudioScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using namespace silencer::clay_bridge;

	Config & cfg = Config::GetInstance();
	CLAY({ .id = CLAY_ID("OptionsAudioRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .padding = { 0, 0, 80, 0 },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsAudioPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(options_audio_screen_detail::kPanelW),
		                       CLAY_SIZING_FIT(0) },
		           .padding = { options_audio_screen_detail::kPanelPadX, options_audio_screen_detail::kPanelPadX,
		                        options_audio_screen_detail::kPanelPadY, options_audio_screen_detail::kPanelPadY },
		           .childGap = 22,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			options_audio_screen_detail::BankText(CLAY_STRING("Audio Options"), options_audio_screen_detail::BankTextVariant::Title, {});
			options_audio_screen_detail::ToggleRow(CLAY_STRING("Music"), cfg.music, options_audio_screen_detail::kActionMusic, interactions);
			CLAY({ .id = CLAY_ID("OptionsAudioActions"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = options_audio_screen_detail::kActionGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				options_audio_screen_detail::BankButton(CLAY_STRING("Save"), options_audio_screen_detail::BankButtonVariant::Chrome, {},
				           options_audio_screen_detail::BankButtonHandle{ nullptr, options_audio_screen_detail::kActionSave, &interactions });
				options_audio_screen_detail::BankButton(CLAY_STRING("Cancel"), options_audio_screen_detail::BankButtonVariant::Chrome, {},
				           options_audio_screen_detail::BankButtonHandle{ nullptr, options_audio_screen_detail::kActionCancel, &interactions });
			}
		}
	}
}

void OptionsAudioScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsAudioScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		cancelClicked = true;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_audio_screen_detail::kActionMusic){
		musicClicked = true;
		return true;
	}
	if(action.id == options_audio_screen_detail::kActionSave){
		saveClicked = true;
		return true;
	}
	if(action.id == options_audio_screen_detail::kActionCancel){
		cancelClicked = true;
		return true;
	}
	return false;
}
