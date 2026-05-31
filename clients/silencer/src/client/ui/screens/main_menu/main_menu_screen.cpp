#include "main_menu_screen.h"

#include "client/ui/screens/main_menu/main_menu_view.h"
#include "screen_context.h"
#include "game_state.h"
#include "renderer.h"
#include "world.h"

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

	tutorialClicked = false;
	lobbyClicked = false;
	optionsClicked = false;
	exitClicked = false;
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

bool MainMenuScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;
	versionText_ = "Silencer v";
	versionText_ += ctx.world.GetVersion();

	*out = silencer::client_ui::MainMenuView(
		silencer::client_ui::MainMenuViewProps{
			.key = "main-menu",
			.version = versionText_.c_str(),
			.on_tutorial = [this](const ::ui::ActivationEvent&) {
				tutorialClicked = true;
			},
			.on_lobby = [this](const ::ui::ActivationEvent&) {
				lobbyClicked = true;
			},
			.on_options = [this](const ::ui::ActivationEvent&) {
				optionsClicked = true;
			},
			.on_exit = [this](const ::ui::ActivationEvent&) {
				exitClicked = true;
			},
		});
	return true;
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
