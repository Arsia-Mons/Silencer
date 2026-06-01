#include "main_menu_screen.h"

#include "client/ui/hooks/use_app.h"
#include "client/ui/hooks/use_game_session.h"
#include "client/ui/hooks/use_navigation.h"
#include "client/ui/screens/main_menu/main_menu_frame.h"
#include "lobby_connect_screen.h"
#include "options_screen.h"
#include "screen_context.h"
#include "clay_ui_compositor.h"
#include "renderer.h"
#include "surface.h"

#include "runtime/UiInteractionRegistry.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <memory>
#include <string>

namespace main_menu_screen_detail
{
constexpr const char * kActionTutorial = "main_menu.tutorial";
constexpr const char * kActionLobby = "main_menu.lobby";
constexpr const char * kActionOptions = "main_menu.options";
constexpr const char * kActionExit = "main_menu.exit";
} // namespace main_menu_screen_detail

void MainMenuScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	// The authored cppx frame owns the visible menu layout and publishes its
	// retained bounds to the existing control-socket interaction registry.

	tutorialClicked = false;
	lobbyClicked = false;
	optionsClicked = false;
	exitClicked = false;
}

void MainMenuScreen::Tick(ScreenContext & ctx)
{
	if(tutorialClicked){
		tutorialClicked = false;
		silencer::client_ui::use_game_session(
			silencer::client_ui::MakeGameSessionProvider(ctx))
			.tutorial.start();
		return;
	}
	if(lobbyClicked){
		lobbyClicked = false;
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<LobbyConnectScreen>());
		return;
	}
	if(optionsClicked){
		optionsClicked = false;
		silencer::client_ui::use_navigation()
			.push(std::make_unique<OptionsScreen>());
		return;
	}
	if(exitClicked){
		exitClicked = false;
		silencer::client_ui::use_app(
			silencer::client_ui::MakeAppProvider(ctx))
			.lifecycle.quit();
		return;
	}
}

void MainMenuScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	silencer::client_ui::AppModel app =
		silencer::client_ui::use_app(
			silencer::client_ui::MakeAppProvider(ctx));
	versionText_ = "Silencer v";
	versionText_ += app.version();

	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));
	silencer::client_ui::MainMenuFrameProps props{
		.key = "main-menu",
		.version = versionText_.c_str(),
		.start_tutorial = [this]() { tutorialClicked = true; },
		.open_lobby = [this]() { lobbyClicked = true; },
		.open_options = [this]() { optionsClicked = true; },
		.quit = [this]() { exitClicked = true; },
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::MainMenuFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);
}

void MainMenuScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MainMenuScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		silencer::client_ui::use_app(
			silencer::client_ui::MakeAppProvider(ctx))
			.lifecycle.quit();
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

const ::ui::DrawCommandList * MainMenuScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
