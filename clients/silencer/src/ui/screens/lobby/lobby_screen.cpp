#include "lobby_screen.h"

#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "interface.h"
#include "objecttypes.h"
#include "game_select_panel.h"

LobbyScreen::LobbyScreen() = default;
LobbyScreen::~LobbyScreen() = default;

void LobbyScreen::Build(ScreenContext & ctx)
{
	Interface * lobbyiface = ctx.game.CreateLobbyInterface();
	ctx.game.lobbyinterface = lobbyiface->id;
	interfaceId = lobbyiface->id;
	character.Build(ctx, lobbyiface);
	chat.Build(ctx, lobbyiface);
	gameSelect = std::unique_ptr<GameSelectPanel>(new GameSelectPanel(*this));
	gameSelect->Build(ctx, lobbyiface);
}

void LobbyScreen::Tick(ScreenContext & ctx)
{
	// Panels first so their handlers see fresh button->clicked /
	// textinput->enterpressed flags before the legacy ProcessLobbyInterface
	// recursive walk clears them.
	character.Tick(ctx);
	chat.Tick(ctx);
	if(gameSelect) gameSelect->Tick(ctx);
	ctx.game.TickLobbyBody();
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	character.Destroy(ctx);
	chat.Destroy(ctx);
	if(gameSelect){
		gameSelect->Destroy(ctx);
		gameSelect.reset();
	}
}

void LobbyScreen::ShowGameSelect(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;
	gameSelect = std::unique_ptr<GameSelectPanel>(new GameSelectPanel(*this));
	gameSelect->Build(ctx, lobbyiface);
}

void LobbyScreen::ShowGameCreate(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;

	// Tear down the GameSelect panel.
	if(gameSelect){
		Interface * panelIface = (Interface *)ctx.world.GetObjectFromId(gameSelect->interfaceId);
		if(panelIface){
			panelIface->DestroyInterface(ctx.world, lobbyiface);
		}
		gameSelect.reset();
		ctx.game.gameselectinterface = 0;
	}

	// Stage D: GameCreate is still the legacy iface; Stage E swaps it to a
	// GameCreatePanel and this branch becomes a panel construction.
	Interface * gamecreateiface = ctx.game.CreateGameCreateInterface();
	ctx.game.gamecreateinterface = gamecreateiface->id;
	lobbyiface->AddObject(gamecreateiface->id);
	lobbyiface->activeobject = gamecreateiface->id;
	Interface * chatiface = (Interface *)ctx.world.GetObjectFromId(ctx.game.chatinterface);
	if(chatiface){
		chatiface->activeobject = 0;
	}
	lobbyiface->ActiveChanged(ctx.world, lobbyiface, false);
	ctx.game.currentinterface = interfaceId;
}
