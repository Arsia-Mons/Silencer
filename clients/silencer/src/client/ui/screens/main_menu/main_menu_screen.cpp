#include "main_menu_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "world.h"
#include "surface.h"

#include "layout/ui_document_renderer.h"
#include "runtime/UiInteractionRegistry.h"
#include "ui_document_assets.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <string>

namespace main_menu_screen_detail
{
constexpr const char * kSurface = "main-menu";
constexpr const char * kLogoComponent = "main-menu.logo";
constexpr const char * kVersionBinding = "client.version";
constexpr const char * kActionTutorial = "main_menu.tutorial";
constexpr const char * kActionLobby = "main_menu.lobby";
constexpr const char * kActionOptions = "main_menu.options";
constexpr const char * kActionExit = "main_menu.exit";
} // namespace main_menu_screen_detail

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
		main_menu_screen_detail::kSurface,
		layoutDocument_,
		layoutLoadError_);
	if(!layoutLoaded_){
		std::fprintf(stderr, "[ui-layout] %s\n", layoutLoadError_.c_str());
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

	silencer::client_ui::UiDocumentRendererOptions options;
	options.buildComponent = [&](const silencer::ui::UiEditorNode& node) {
		if(node.component != main_menu_screen_detail::kLogoComponent) return false;
		logo.Build(ctx.world.resources);
		return true;
	};
	options.resolveTextBinding = [&](const std::string& binding) {
		if(binding == main_menu_screen_detail::kVersionBinding) return versionText_;
		return std::string();
	};
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
	if(action.id == main_menu_screen_detail::kActionTutorial){
		tutorialClicked = true;
		return true;
	}
	if(action.id == main_menu_screen_detail::kActionLobby){
		lobbyClicked = true;
		return true;
	}
	if(action.id == main_menu_screen_detail::kActionOptions){
		optionsClicked = true;
		return true;
	}
	if(action.id == main_menu_screen_detail::kActionExit){
		exitClicked = true;
		return true;
	}
	return false;
}
