#include "options_display_screen.h"

#include "options_document_runtime.h"
#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"
#include "config.h"
#include "renderdevice.h"

#include "components/boolean_setting_row.h"
#include "layout/ui_document_renderer.h"
#include "layout/ui_document_runtime_registry.h"
#include "runtime/UiInteractionRegistry.h"
#include "ui_document_assets.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

#include <cstdio>

namespace options_display_screen_detail
{

Clay_String ClayStringFromStd(const std::string& value)
{
	return Clay_String{ false, static_cast<int32_t>(value.size()), value.c_str() };
}

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

bool BuildDisplayComponent(const silencer::ui::UiEditorNode& node,
                           silencer::ui::UiInteractionRegistry& interactions)
{
	Config & cfg = Config::GetInstance();
	const std::string rowId = node.id + "Content";
	if(node.component == silencer::client_ui::options_display::kComponentFullscreenRow){
		silencer::client_ui::options::BooleanSettingRow(
			ClayStringFromStd(rowId),
			CLAY_STRING("OptionsDisplayFullscreenButton"),
			CLAY_STRING("Fullscreen"),
			cfg.fullscreen,
			silencer::client_ui::options_display::kActionFullscreen,
			interactions);
		return true;
	}
	if(node.component == silencer::client_ui::options_display::kComponentSmoothScalingRow){
		silencer::client_ui::options::BooleanSettingRow(
			ClayStringFromStd(rowId),
			CLAY_STRING("OptionsDisplaySmoothScalingButton"),
			CLAY_STRING("Smooth Scaling"),
			cfg.scalefilter,
			silencer::client_ui::options_display::kActionSmoothScaling,
			interactions);
		return true;
	}
	return false;
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
	options.buildComponent = [&interactions](const silencer::ui::UiEditorNode& node) {
		return options_display_screen_detail::BuildDisplayComponent(node, interactions);
	};
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
	if(action.id == silencer::client_ui::options_display::kActionFullscreen){
		fullscreenClicked = true;
		return true;
	}
	if(action.id == silencer::client_ui::options_display::kActionSmoothScaling){
		smoothScalingClicked = true;
		return true;
	}
	if(action.id == silencer::client_ui::options_display::kActionSave){
		saveClicked = true;
		return true;
	}
	if(action.id == silencer::client_ui::options_display::kActionCancel){
		cancelClicked = true;
		return true;
	}
	return false;
}
