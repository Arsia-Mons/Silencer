#include "main_menu_screen.h"

#include "client/ui/screens/main_menu/main_menu_view.h"
#include "screen_context.h"
#include "lobby.h"
#include "peer.h"
#include "renderer.h"
#include "world.h"

#include <string>

void MainMenuScreen::Build(ScreenContext & ctx)
{
	ctx.world.Disconnect();
	ctx.world.ClearGameplayState();
	ctx.world.lobby.Disconnect();
	ctx.UnloadGame();
	ctx.world.GetAuthorityPeer()->controlledlist.clear();
	ctx.world.DestroyAllObjects();
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
}

void MainMenuScreen::Tick(ScreenContext & ctx)
{
	ctx.PlayMenuMusicIfReady();
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
			.resources = &ctx.world.resources,
		},
		silencer::client_ui::MainMenuView);
	return true;
}

void MainMenuScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MainMenuScreen::HandleBack(ScreenContext & ctx)
{
	ctx.RequestQuit();
	return true;
}
