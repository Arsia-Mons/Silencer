#include "lobby_screen.h"

#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "interface.h"
#include "objecttypes.h"
#include "game_select_panel.h"
#include "game_create_panel.h"
#include "game_join_panel.h"

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
	if(gameJoin)   gameJoin->Tick(ctx);
	ctx.game.TickLobbyBody();

	// TickLobbyBody tears down gameselectinterface / gamecreateinterface
	// when the CONNECTED transition fires (Join/Create handoff into a game).
	// Game::GoBack tears down gamejoininterface when leaving the joined
	// game. Drop the matching panel objects so we don't tick stale ifaces
	// next frame and so subsequent ShowGameSelect/ShowGameCreate/ShowGameJoin
	// rebuilds start fresh.
	if(gameSelect && ctx.game.gameselectinterface == 0){
		gameSelect.reset();
	}
	if(gameCreate && ctx.game.gamecreateinterface == 0){
		gameCreate.reset();
	}
	if(gameJoin && ctx.game.gamejoininterface == 0){
		gameJoin.reset();
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
	if(gameJoin){
		gameJoin->Destroy(ctx);
		gameJoin.reset();
	}
}

// Tear down any active right-side panel + matching mirror id on Game.
// Called by every Show* swap helper before building the new panel.
static void TearDownRightPanels(LobbyScreen & self, ScreenContext & ctx, Interface * lobbyiface,
                                std::unique_ptr<GameSelectPanel> & gameSelect,
                                std::unique_ptr<GameCreatePanel> & gameCreate,
                                std::unique_ptr<GameJoinPanel>   & gameJoin)
{
	(void)self;
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
	if(gameJoin){
		Interface * panelIface = (Interface *)ctx.world.GetObjectFromId(gameJoin->interfaceId);
		if(panelIface) panelIface->DestroyInterface(ctx.world, lobbyiface);
		gameJoin.reset();
		ctx.game.gamejoininterface = 0;
	}
	// Tear down legacy gametechinterface too (Stage G replaces with panel).
	if(ctx.game.gametechinterface){
		Interface * gametechiface = (Interface *)ctx.world.GetObjectFromId(ctx.game.gametechinterface);
		if(gametechiface) gametechiface->DestroyInterface(ctx.world, lobbyiface);
		ctx.game.gametechinterface = 0;
		ctx.world.choosingtech = false;
		ctx.game.ShowTeamOverlays(true);
	}
}

void LobbyScreen::ShowGameSelect(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;
	TearDownRightPanels(*this, ctx, lobbyiface, gameSelect, gameCreate, gameJoin);
	gameSelect = std::unique_ptr<GameSelectPanel>(new GameSelectPanel(*this));
	gameSelect->Build(ctx, lobbyiface);
}

void LobbyScreen::ShowGameCreate(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;
	TearDownRightPanels(*this, ctx, lobbyiface, gameSelect, gameCreate, gameJoin);

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

void LobbyScreen::ShowGameJoin(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;
	TearDownRightPanels(*this, ctx, lobbyiface, gameSelect, gameCreate, gameJoin);

	gameJoin = std::unique_ptr<GameJoinPanel>(new GameJoinPanel(*this));
	gameJoin->Build(ctx, lobbyiface);
	// CONNECTED transition (Join/Create handoff) doesn't reassign
	// activeobject; the legacy CreateGameJoinInterface call only added the
	// iface as a child of the lobby iface. Match that behavior.
}

void LobbyScreen::ShowGameTech(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;
	TearDownRightPanels(*this, ctx, lobbyiface, gameSelect, gameCreate, gameJoin);

	// Stage G replaces this with a GameTechPanel build; for Stage F we keep
	// the legacy CreateGameTechInterface so the swap behavior is byte-for-
	// byte identical to the pre-refactor case-27 button handler.
	ctx.world.choosingtech = true;
	ctx.game.ShowTeamOverlays(false);
	ctx.world.RequestPeerList();
	ctx.game.gametechinterface = ctx.game.CreateGameTechInterface()->id;
	lobbyiface->AddObject(ctx.game.gametechinterface);
	lobbyiface->activeobject = ctx.game.gametechinterface;
	Interface * chatiface = (Interface *)ctx.world.GetObjectFromId(ctx.game.chatinterface);
	if(chatiface){
		chatiface->activeobject = 0;
	}
	ctx.game.currentinterface = interfaceId;
	lobbyiface->ActiveChanged(ctx.world, lobbyiface, false);
	ctx.game.UpdateTechInterface();
}
