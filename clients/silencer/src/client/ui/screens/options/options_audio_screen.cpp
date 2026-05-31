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
	const silencer::client_ui::OptionsAudioContextValue context{
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
	const auto * stored = ::ui::copy_value(context);
	if(!stored) return false;
	*out = ::ui::component(
		"OptionsAudioView",
		silencer::client_ui::OptionsAudioViewProps{
			.key = "options-audio",
			.value = stored,
		},
		silencer::client_ui::OptionsAudioView);
	return true;
}

void OptionsAudioScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsAudioScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		options_audio_screen_detail::CancelAudioSettings();
		ctx.GoToState(GameState::OPTIONS);
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == options_audio_screen_detail::kActionMusic){
		options_audio_screen_detail::ToggleMusicSetting();
		return true;
	}
	if(action.id == options_audio_screen_detail::kActionSave){
		options_audio_screen_detail::SaveAudioSettings();
		ctx.GoToState(GameState::OPTIONS);
		return true;
	}
	if(action.id == options_audio_screen_detail::kActionCancel){
		options_audio_screen_detail::CancelAudioSettings();
		ctx.GoToState(GameState::OPTIONS);
		return true;
	}
	return false;
}
