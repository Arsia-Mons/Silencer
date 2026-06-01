#include "options_audio_screen.h"

#include "client/ui/screens/options/options_audio_view.h"
#include "screen_context.h"
#include "renderer.h"
#include "config.h"
#include "audio.h"
#include "world.h"

namespace options_audio_screen_detail
{
void ApplyMusicSetting(bool on)
{
	if(on){
		Audio::GetInstance().ResumeMusic();
	}else{
		Audio::GetInstance().PauseMusic();
	}
}

void ToggleMusicSetting()
{
	Config & cfg = Config::GetInstance();
	cfg.music = !cfg.music;
	ApplyMusicSetting(cfg.music);
}

void SaveAudioSettings()
{
	Config::GetInstance().Save();
}

void CancelAudioSettings()
{
	Config & cfg = Config::GetInstance();
	cfg.Load();
	ApplyMusicSetting(cfg.music);
}
} // namespace options_audio_screen_detail

void OptionsAudioScreen::Build(ScreenContext & ctx)
{
	ctx.world.DestroyAllObjects();
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
}

void OptionsAudioScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsAudioScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	(void)ctx;
	if(!out) return false;

	Config & cfg = Config::GetInstance();
	const silencer::client_ui::OptionsAudio audio{
		.music = cfg.music,
		.toggle_music = []() {
			options_audio_screen_detail::ToggleMusicSetting();
		},
		.save = []() {
			options_audio_screen_detail::SaveAudioSettings();
		},
		.cancel = []() {
			options_audio_screen_detail::CancelAudioSettings();
		},
	};
	const auto * stored = ::ui::copy_value(audio);
	if(!stored) return false;
	*out = ::ui::component(
		"OptionsAudioView",
		silencer::client_ui::OptionsAudioViewProps{
			.key = "options-audio",
			.audio = stored,
		},
		silencer::client_ui::OptionsAudioView);
	return true;
}

void OptionsAudioScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsAudioScreen::HandleBack(ScreenContext & ctx)
{
	options_audio_screen_detail::CancelAudioSettings();
	ctx.PopScreen();
	return true;
}
