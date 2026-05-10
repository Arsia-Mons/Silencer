#include "lobby_screen.h"

#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "interface.h"
#include "objecttypes.h"
#include "game_select_panel.h"
#include "game_create_panel.h"

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
	if(gameCreate) gameCreate->Tick(ctx);
	ctx.game.TickLobbyBody();

	// TickLobbyBody tears down gameselectinterface / gamecreateinterface
	// when the CONNECTED transition fires (Join/Create handoff into a game).
	// Drop the matching panel objects so we don't tick stale ifaces next
	// frame and so a subsequent ShowGameSelect/ShowGameCreate rebuild starts
	// fresh.
	if(gameSelect && ctx.game.gameselectinterface == 0){
		gameSelect.reset();
	}
	if(gameCreate && ctx.game.gamecreateinterface == 0){
		gameCreate.reset();
	}
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	character.Destroy(ctx);
	chat.Destroy(ctx);
	if(gameSelect){
		gameSelect->Destroy(ctx);
		gameSelect.reset();
	}
	if(gameCreate){
		gameCreate->Destroy(ctx);
		gameCreate.reset();
	}
}

void LobbyScreen::ShowGameSelect(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;
	// Tear down whichever right-side panel is currently active before
	// building a fresh GameSelectPanel.
	if(gameSelect){
		Interface * panelIface = (Interface *)ctx.world.GetObjectFromId(gameSelect->interfaceId);
		if(panelIface) panelIface->DestroyInterface(ctx.world, lobbyiface);
		gameSelect.reset();
		ctx.game.gameselectinterface = 0;
	}
	if(gameCreate){
		Interface * panelIface = (Interface *)ctx.world.GetObjectFromId(gameCreate->interfaceId);
		if(panelIface) panelIface->DestroyInterface(ctx.world, lobbyiface);
		gameCreate.reset();
		ctx.game.gamecreateinterface = 0;
	}
	gameSelect = std::unique_ptr<GameSelectPanel>(new GameSelectPanel(*this));
	gameSelect->Build(ctx, lobbyiface);
}

void LobbyScreen::ShowGameCreate(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;

	// Tear down whichever right-side panel is active.
	if(gameSelect){
		Interface * panelIface = (Interface *)ctx.world.GetObjectFromId(gameSelect->interfaceId);
		if(panelIface){
			panelIface->DestroyInterface(ctx.world, lobbyiface);
		}
		gameSelect.reset();
		ctx.game.gameselectinterface = 0;
	}
	if(gameCreate){
		Interface * panelIface = (Interface *)ctx.world.GetObjectFromId(gameCreate->interfaceId);
		if(panelIface){
			panelIface->DestroyInterface(ctx.world, lobbyiface);
		}
		gameCreate.reset();
		ctx.game.gamecreateinterface = 0;
	}

	gameCreate = std::unique_ptr<GameCreatePanel>(new GameCreatePanel(*this));
	gameCreate->Build(ctx, lobbyiface);

	lobbyiface->activeobject = ctx.game.gamecreateinterface;
	Interface * chatiface = (Interface *)ctx.world.GetObjectFromId(ctx.game.chatinterface);
	if(chatiface){
		chatiface->activeobject = 0;
	}
	lobbyiface->ActiveChanged(ctx.world, lobbyiface, false);
	ctx.game.currentinterface = interfaceId;
}
