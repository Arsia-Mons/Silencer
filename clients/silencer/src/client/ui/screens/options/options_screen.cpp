#include "options_screen.h"

#include "options_document_runtime.h"
#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "world.h"
#include "surface.h"

#include "layout/ui_document_renderer.h"
#include "layout/ui_document_runtime_registry.h"
#include "runtime/UiInteractionRegistry.h"
#include "ui_document_assets.h"

#include <SDL3/SDL.h>

#include <cstdio>

void OptionsScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	// Clay owns all visible options-menu structure and hit targets.

	goBackClicked = false;
	controlsClicked = false;
	displayClicked = false;
	audioClicked = false;
	layoutLoaded_ = silencer::net::LoadUiDocumentAsset(
		silencer::client_ui::options_menu::kOptionsSurface,
		layoutDocument_,
		layoutLoadError_);
	if(!layoutLoaded_){
		std::fprintf(stderr, "[ui-layout] %s\n", layoutLoadError_.c_str());
		return;
	}
	silencer::client_ui::UiDocumentRendererOptions validationOptions =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::options_menu::kOptionsSurface);
	if(!silencer::client_ui::ValidateUiDocumentRuntimeTokens(
		   layoutDocument_, validationOptions, layoutLoadError_)){
		layoutLoaded_ = false;
		std::fprintf(stderr, "[ui-layout] %s\n", layoutLoadError_.c_str());
		return;
	}
}

void OptionsScreen::Tick(ScreenContext & ctx)
{
	if(goBackClicked){
		goBackClicked = false;
		ctx.GoToState(GameState::MAINMENU);
		return;
	}
	if(controlsClicked){
		controlsClicked = false;
		ctx.GoToState(GameState::OPTIONSCONTROLS);
		return;
	}
	if(displayClicked){
		displayClicked = false;
		ctx.GoToState(GameState::OPTIONSDISPLAY);
		return;
	}
	if(audioClicked){
		audioClicked = false;
		ctx.GoToState(GameState::OPTIONSAUDIO);
		return;
	}
}

void OptionsScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	if(!layoutLoaded_) return;

	silencer::client_ui::UiDocumentRendererOptions options =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::options_menu::kOptionsSurface);
	silencer::client_ui::BuildUiDocument(layoutDocument_, interactions, options);
}

void OptionsScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		goBackClicked = true;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	(void)ctx;
	return silencer::client_ui::options_menu::HandleOptionsMenuAction(
		action.id,
		[this](silencer::client_ui::options_menu::OptionsMenuAction menuAction) {
			switch(menuAction){
				case silencer::client_ui::options_menu::OptionsMenuAction::Controls:
					controlsClicked = true;
					return true;
				case silencer::client_ui::options_menu::OptionsMenuAction::Display:
					displayClicked = true;
					return true;
				case silencer::client_ui::options_menu::OptionsMenuAction::Audio:
					audioClicked = true;
					return true;
				case silencer::client_ui::options_menu::OptionsMenuAction::Back:
					goBackClicked = true;
					return true;
			}
			return false;
		});
}
