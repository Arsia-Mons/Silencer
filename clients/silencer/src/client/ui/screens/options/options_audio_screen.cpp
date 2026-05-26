#include "options_audio_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "surface.h"
#include "config.h"
#include "audio.h"

#include "components/boolean_setting_row.h"
#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"

#include <SDL3/SDL.h>

namespace options_audio_screen_detail
{
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;

constexpr uint16_t kPanelW = 420;
constexpr uint16_t kPanelPadX = 24;
constexpr uint16_t kPanelPadY = 32;
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
} // namespace options_audio_screen_detail

void OptionsAudioScreen::Build(ScreenContext & ctx)
{
	ctx.ResetMenuPresentation(1);
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
			options_audio_screen_detail::Text(CLAY_STRING("Audio Options"),
			                                  { .size = options_audio_screen_detail::TextSize::Title });
			silencer::client_ui::options::BooleanSettingRow(
				CLAY_STRING("OptionsAudioMusicRow"),
				CLAY_STRING("OptionsAudioMusicButton"),
				CLAY_STRING("Music"),
				cfg.music,
				options_audio_screen_detail::kActionMusic,
				interactions);
			CLAY({ .id = CLAY_ID("OptionsAudioActions"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = options_audio_screen_detail::kActionGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				options_audio_screen_detail::Button(CLAY_STRING("OptionsAudioSaveButton"), CLAY_STRING("Save"),
				           options_audio_screen_detail::ButtonOpts{ .variant = options_audio_screen_detail::ButtonVariant::Oval,
				                                                    .size = options_audio_screen_detail::ButtonSize::Md },
				           options_audio_screen_detail::ButtonHandle{ nullptr, options_audio_screen_detail::kActionSave, &interactions });
				options_audio_screen_detail::Button(CLAY_STRING("OptionsAudioCancelButton"), CLAY_STRING("Cancel"),
				           options_audio_screen_detail::ButtonOpts{ .variant = options_audio_screen_detail::ButtonVariant::Oval,
				                                                    .size = options_audio_screen_detail::ButtonSize::Md },
				           options_audio_screen_detail::ButtonHandle{ nullptr, options_audio_screen_detail::kActionCancel, &interactions });
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
