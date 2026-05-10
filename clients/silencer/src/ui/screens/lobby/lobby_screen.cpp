#include "lobby_screen.h"

#include "screen_context.h"
#include "world.h"
#include "interface.h"
#include "objecttypes.h"

void LobbyScreen::Build(ScreenContext & ctx)
{
	interfaceId = ctx.BuildLegacyLobbyInterface();
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	character.Build(ctx, lobbyiface);
}

void LobbyScreen::Tick(ScreenContext & ctx)
{
	ctx.TickLegacyLobbyBody();
	character.Tick(ctx);
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	character.Destroy(ctx);
}
