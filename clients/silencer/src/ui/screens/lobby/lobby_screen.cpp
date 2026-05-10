#include "lobby_screen.h"

#include "screen_context.h"

void LobbyScreen::Build(ScreenContext & ctx)
{
	interfaceId = ctx.BuildLegacyLobbyInterface();
}

void LobbyScreen::Tick(ScreenContext & ctx)
{
	ctx.TickLegacyLobbyBody();
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
