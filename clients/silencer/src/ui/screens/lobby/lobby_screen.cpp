#include "lobby_screen.h"

#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "interface.h"
#include "objecttypes.h"

void LobbyScreen::Build(ScreenContext & ctx)
{
	Interface * lobbyiface = ctx.game.CreateLobbyInterface();
	ctx.game.lobbyinterface = lobbyiface->id;
	interfaceId = lobbyiface->id;
	character.Build(ctx, lobbyiface);
	chat.Build(ctx, lobbyiface);
}

void LobbyScreen::Tick(ScreenContext & ctx)
{
	ctx.game.TickLobbyBody();
	character.Tick(ctx);
	chat.Tick(ctx);
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	character.Destroy(ctx);
	chat.Destroy(ctx);
}
