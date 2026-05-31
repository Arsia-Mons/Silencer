#include "options_audio_screen.h"

#include "client/ui/screens/options/options_audio_view.h"
#include "screen_context.h"
#include "game_state.h"
#include "renderer.h"
#include "config.h"
#include "audio.h"

namespace options_audio_screen_detail
{
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

bool OptionsAudioScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	(void)ctx;
	if(!out) return false;

	Config & cfg = Config::GetInstance();
	*out = silencer::client_ui::OptionsAudioView(
		silencer::client_ui::OptionsAudioViewProps{
			.key = "options-audio",
			.music = cfg.music,
			.on_music = [this](bool) {
				musicClicked = true;
			},
			.on_save = [this](const ::ui::ActivationEvent&) {
				saveClicked = true;
			},
			.on_cancel = [this](const ::ui::ActivationEvent&) {
				cancelClicked = true;
			},
		});
	return true;
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
