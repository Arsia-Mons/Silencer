#include "options_screen.h"

#include "client/ui/screens/options/options_menu_view.h"
#include "screen_context.h"
#include "game_state.h"
#include "renderer.h"
#include "world.h"

void OptionsScreen::Build(ScreenContext & ctx)
{
	ctx.world.DestroyAllObjects();
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
}

void OptionsScreen::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	(void)ctx;
	if(!out) return false;
	*out = ::ui::component(
		"OptionsMenuView",
		silencer::client_ui::OptionsMenuViewProps{
			.key = "options-menu",
		},
		silencer::client_ui::OptionsMenuView);
	return true;
}

void OptionsScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool OptionsScreen::HandleBack(ScreenContext & ctx)
{
	ctx.GoToState(GameState::MAINMENU);
	return true;
}
