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
}

void MainMenuScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

bool MainMenuScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;
	versionText_ = "Silencer v";
	versionText_ += ctx.world.GetVersion();

	*out = ::ui::component(
		"MainMenuView",
		silencer::client_ui::MainMenuViewProps{
			.key = "main-menu",
			.version = versionText_.c_str(),
		},
		silencer::client_ui::MainMenuView);
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
		ctx.GoToState(GameState::SINGLEPLAYERGAME);
		return true;
	}
	if(action.id == main_menu_screen_detail::kActionLobby){
		ctx.GoToState(GameState::LOBBYCONNECT);
		return true;
	}
	if(action.id == main_menu_screen_detail::kActionOptions){
		ctx.GoToState(GameState::OPTIONS);
		return true;
	}
	if(action.id == main_menu_screen_detail::kActionExit){
		ctx.RequestQuit();
		return true;
	}
	return false;
}
