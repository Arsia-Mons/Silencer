#include "options_audio_screen.h"

#include "options_document_runtime.h"
#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"
#include "config.h"
#include "audio.h"

#include "layout/ui_document_renderer.h"
#include "layout/ui_document_runtime_registry.h"
#include "runtime/UiInteractionRegistry.h"
#include "ui_document_assets.h"

#include <SDL3/SDL.h>

#include <cstdio>

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

bool LoadAudioDocument(silencer::ui::UiEditorPreviewDocument& document,
                       std::string& error)
{
	if(!silencer::net::LoadUiDocumentAsset(
		   silencer::client_ui::options_audio::kOptionsAudioSurface,
		   document,
		   error)){
		return false;
	}
	silencer::client_ui::UiDocumentRendererOptions options =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::options_audio::kOptionsAudioSurface);
	return silencer::client_ui::ValidateUiDocumentRuntimeTokens(
		document,
		options,
		error);
}
} // namespace options_audio_screen_detail

void OptionsAudioScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	musicClicked = false;
	saveClicked = false;
	cancelClicked = false;
	layoutLoaded_ = options_audio_screen_detail::LoadAudioDocument(
		layoutDocument_,
		layoutLoadError_);
	if(!layoutLoaded_){
		std::fprintf(stderr, "[ui-layout] %s\n", layoutLoadError_.c_str());
	}
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
	(void)ctx;
	(void)frametime;
	(void)dst;
	if(!layoutLoaded_) return;

	silencer::client_ui::UiDocumentRendererOptions options =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::options_audio::kOptionsAudioSurface);
	silencer::client_ui::BuildUiDocument(layoutDocument_, interactions, options);
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
	if(action.id == silencer::client_ui::options_audio::kActionMusic){
		musicClicked = true;
		return true;
	}
	if(action.id == silencer::client_ui::options_audio::kActionSave){
		saveClicked = true;
		return true;
	}
	if(action.id == silencer::client_ui::options_audio::kActionCancel){
		cancelClicked = true;
		return true;
	}
	return false;
}
