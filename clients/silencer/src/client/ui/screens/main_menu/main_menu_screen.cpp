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

#include "runtime/UiInteractionRegistry.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <memory>
#include <string>

void MainMenuScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);

	// The authored cppx frame owns the visible menu layout and publishes its
	// retained bounds to the existing control-socket interaction registry.
}

void MainMenuScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

void MainMenuScreen::BuildUi(ScreenContext & ctx, float frametime, const silencer::ui::UiInputState& input, Uint8, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	silencer::client_ui::AppModel app =
		silencer::client_ui::use_app(
			silencer::client_ui::MakeAppProvider(ctx));
	silencer::client_ui::GameSessionModel session =
		silencer::client_ui::use_game_session(
			silencer::client_ui::MakeGameSessionProvider(ctx));
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();
	versionText_ = "Silencer v";
	versionText_ += app.version();

	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, input.width);
	const int virtualH = std::max(1, input.height);
	silencer::client_ui::MainMenuFrameProps props{
		.key = "main-menu",
		.version = versionText_.c_str(),
		.start_tutorial = [session]() { session.tutorial.start(); },
		.open_lobby = [navigation]() {
			navigation.reset_to(std::make_unique<LobbyConnectScreen>());
		},
		.open_options = [navigation]() {
			navigation.push(std::make_unique<OptionsScreen>());
		},
		.quit = [app]() { app.lifecycle.quit(); },
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
	return retainedFrame_.HandleUiIntent(action);
}

const ::ui::DrawCommandList * MainMenuScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
