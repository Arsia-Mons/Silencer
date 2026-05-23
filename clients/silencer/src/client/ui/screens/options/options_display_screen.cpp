#include "options_display_screen.h"

#include "options_document_runtime.h"
#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"
#include "config.h"
#include "renderdevice.h"

#include "layout/ui_document_renderer.h"
#include "layout/ui_document_runtime_registry.h"
#include "runtime/UiInteractionRegistry.h"
#include "ui_document_assets.h"

#include <SDL3/SDL.h>
#include <cstdio>

namespace options_display_screen_detail
{

bool LoadDisplayDocument(silencer::ui::UiEditorPreviewDocument& document,
                         std::string& error)
{
	if(!silencer::net::LoadUiDocumentAsset(
		   silencer::client_ui::options_display::kOptionsDisplaySurface,
		   document,
		   error)){
		return false;
	}
	silencer::client_ui::UiDocumentRendererOptions options =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::options_display::kOptionsDisplaySurface);
	return silencer::client_ui::ValidateUiDocumentRuntimeTokens(
		document,
		options,
		error);
}
} // namespace options_display_screen_detail

void OptionsDisplayScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	fullscreenClicked = false;
	smoothScalingClicked = false;
	saveClicked = false;
	cancelClicked = false;
	layoutLoaded_ = options_display_screen_detail::LoadDisplayDocument(
		layoutDocument_,
		layoutLoadError_);
	if(!layoutLoaded_){
		std::fprintf(stderr, "[ui-layout] %s\n", layoutLoadError_.c_str());
	}
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
	(void)ctx;
	(void)frametime;
	(void)dst;
	if(!layoutLoaded_) return;

	silencer::client_ui::UiDocumentRendererOptions options =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::options_display::kOptionsDisplaySurface);
	silencer::client_ui::BuildUiDocument(layoutDocument_, interactions, options);
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
	return silencer::client_ui::options_display::HandleOptionsDisplayAction(
		action.id,
		[this](silencer::client_ui::options_display::OptionsDisplayAction displayAction) {
			switch(displayAction){
				case silencer::client_ui::options_display::OptionsDisplayAction::Fullscreen:
					fullscreenClicked = true;
					return true;
				case silencer::client_ui::options_display::OptionsDisplayAction::SmoothScaling:
					smoothScalingClicked = true;
					return true;
				case silencer::client_ui::options_display::OptionsDisplayAction::Save:
					saveClicked = true;
					return true;
				case silencer::client_ui::options_display::OptionsDisplayAction::Cancel:
					cancelClicked = true;
					return true;
			}
			return false;
		});
}
