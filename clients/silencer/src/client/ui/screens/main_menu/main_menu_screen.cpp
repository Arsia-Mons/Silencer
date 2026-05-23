#include "main_menu_screen.h"

#include "main_menu_document_runtime.h"
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
#include <string>

void MainMenuScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	// Clay owns every visible main-menu element. No retained Interface/Object
	// widget graph is built for this screen. Button hit-test bounds are
	// resolved from Clay's real layout (ResolveClayBoundsFromClay), so no
	// absolute coordinates are registered here.

	tutorialClicked = false;
	lobbyClicked = false;
	optionsClicked = false;
	exitClicked = false;
	layoutLoaded_ = silencer::net::LoadUiDocumentAsset(
		silencer::client_ui::main_menu::kMainMenuSurface,
		layoutDocument_,
		layoutLoadError_);
	if(!layoutLoaded_){
		std::fprintf(stderr, "[ui-layout] %s\n", layoutLoadError_.c_str());
		return;
	}
	silencer::client_ui::UiDocumentRendererOptions validationOptions =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::main_menu::kMainMenuSurface);
	if(!silencer::client_ui::ValidateUiDocumentRuntimeTokens(
		   layoutDocument_, validationOptions, layoutLoadError_)){
		layoutLoaded_ = false;
		std::fprintf(stderr, "[ui-layout] %s\n", layoutLoadError_.c_str());
		return;
	}
	logo.Reset();
}

void MainMenuScreen::Tick(ScreenContext & ctx)
{
	if(tutorialClicked){
		tutorialClicked = false;
		ctx.GoToState(GameState::SINGLEPLAYERGAME);
		return;
	}
	if(lobbyClicked){
		lobbyClicked = false;
		ctx.GoToState(GameState::LOBBYCONNECT);
		return;
	}
	if(optionsClicked){
		optionsClicked = false;
		ctx.GoToState(GameState::OPTIONS);
		return;
	}
	if(exitClicked){
		exitClicked = false;
		ctx.RequestQuit();
		return;
	}
}

void MainMenuScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;

	versionText_ = "Silencer v";
	versionText_ += ctx.world.GetVersion();
	if(!layoutLoaded_) return;

	silencer::client_ui::UiDocumentRendererOptions options =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(
			silencer::client_ui::main_menu::kMainMenuSurface);
	silencer::client_ui::main_menu::ApplyMainMenuRuntimeHandlers(
		options,
		&ctx.world.resources,
		&logo,
		&versionText_);
	silencer::client_ui::BuildUiDocument(layoutDocument_, interactions, options);
}

void MainMenuScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MainMenuScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		ctx.RequestQuit();
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	return silencer::client_ui::main_menu::HandleMainMenuAction(
		action.id,
		[this](silencer::client_ui::main_menu::MainMenuAction menuAction) {
			switch(menuAction){
				case silencer::client_ui::main_menu::MainMenuAction::Tutorial:
					tutorialClicked = true;
					return true;
				case silencer::client_ui::main_menu::MainMenuAction::Lobby:
					lobbyClicked = true;
					return true;
				case silencer::client_ui::main_menu::MainMenuAction::Options:
					optionsClicked = true;
					return true;
				case silencer::client_ui::main_menu::MainMenuAction::Exit:
					exitClicked = true;
					return true;
			}
			return false;
		});
}
