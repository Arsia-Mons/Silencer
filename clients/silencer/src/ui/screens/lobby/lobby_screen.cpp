#include "lobby_screen.h"

#include "screen_context.h"
#include "game.h"
#include "game_state.h"
#include "world.h"
#include "lobby.h"
#include "lobbygame.h"
#include "config.h"
#include "interface.h"
#include "overlay.h"
#include "button.h"
#include "objecttypes.h"
#include "ambience_mixer.h"
#include "map_downloader.h"
#include "message_modal.h"
#include "game_select_panel.h"
#include "game_create_panel.h"
#include "game_join_panel.h"
#include "game_tech_panel.h"

#include <cstring>
#include <memory>
#include <string>

namespace
{
MessageModal * TopAsProgressModal(ScreenContext & ctx)
{
	Screen * top = ctx.game.GetTopScreen();
	if(!top) return nullptr;
	MessageModal * m = dynamic_cast<MessageModal *>(top);
	return (m && m->IsProgress()) ? m : nullptr;
}

bool TopIsModal(ScreenContext & ctx)
{
	Screen * top = ctx.game.GetTopScreen();
	return top && top->IsOverlay();
}

void DismissProgressModal(ScreenContext & ctx)
{
	if(TopAsProgressModal(ctx)) ctx.PopScreen();
}
}

LobbyScreen::LobbyScreen() = default;
LobbyScreen::~LobbyScreen() = default;

void LobbyScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;

	Overlay * background = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	background->res_bank = 7;
	background->res_index = 1;
	Overlay * toptext = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	toptext->text = "Silencer";
	toptext->textbank = 135;
	toptext->textwidth = 11;
	toptext->effectcolor = 152;
	toptext->x = 15;
	toptext->y = 32;
	Overlay * vertext = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	vertext->text = "v.";
	vertext->text += world.GetVersion();
	vertext->textbank = 133;
	vertext->textwidth = 6;
	vertext->effectcolor = 189;
	vertext->x = 115;
	vertext->y = 39;
	Overlay * mapnametext = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	mapnametext->textbank = 135;
	mapnametext->textwidth = 11;
	mapnametext->effectcolor = 129;
	mapnametext->effectbrightness = 128 + 32;
	mapnametext->textcolorramp = true;
	mapnametext->x = 180;
	mapnametext->y = 32;
	mapnametext->uid = 8;
	Button * exitbutton = static_cast<Button *>(world.CreateObject(ObjectTypes::BUTTON));
	exitbutton->y = 29;
	exitbutton->x = 473;
	exitbutton->SetType(Button::B156x21);
	exitbutton->uid = 10;
	strcpy(exitbutton->text, "Go Back");

	Interface * lobbyiface = static_cast<Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
	lobbyiface->AddObject(background->id);
	lobbyiface->AddObject(toptext->id);
	lobbyiface->AddObject(vertext->id);
	lobbyiface->AddObject(mapnametext->id);
	lobbyiface->AddObject(exitbutton->id);
	lobbyiface->buttonescape = exitbutton->id;
	interfaceId = lobbyiface->id;
	ctx.game.lobbyinterface = lobbyiface->id;

	character.Build(ctx, lobbyiface);
	chat.Build(ctx, lobbyiface);
	gameSelect = std::unique_ptr<GameSelectPanel>(new GameSelectPanel(*this));
	gameSelect->Build(ctx, lobbyiface);
}

void LobbyScreen::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	Game & game = ctx.game;

	// Lobby disconnect → bounce back to the connect screen.
	if(world.lobby.state == Lobby::DISCONNECTED){
		world.Disconnect();
		ctx.GoToState(GameState::LOBBYCONNECT);
		return;
	}

	// Panels first so their handlers see fresh button->clicked /
	// textinput->enterpressed flags before the lobby-chrome walk below.
	character.Tick(ctx);
	chat.Tick(ctx);
	if(gameSelect) gameSelect->Tick(ctx);
	if(gameCreate) gameCreate->Tick(ctx);
	if(gameJoin)   gameJoin->Tick(ctx);
	if(gameTech)   gameTech->Tick(ctx);

	// Lobby-chrome Go Back button.
	Interface * lobbyiface = static_cast<Interface *>(world.GetObjectFromId(interfaceId));
	if(lobbyiface){
		for(Uint16 child : lobbyiface->objects){
			Object * obj = world.GetObjectFromId(child);
			if(!obj || obj->type != ObjectTypes::BUTTON) continue;
			Button * button = static_cast<Button *>(obj);
			if(button->uid == 10 && button->clicked && button->type != Button::BCHECKBOX){
				button->clicked = false;
				if(game.GoBack()) return;
				break;
			}
		}
	}

	MapDownloader & mapDownloader = ctx.mapDownloader;

	// Deferred CreateGame state machine — kicked off by GameCreatePanel; runs
	// here so it survives the panel teardown that fires on success.
	if(game.gamecreateinterface){
		int us = mapDownloader.mapUploadState.load(std::memory_order_acquire);
		if(us == 2){
			mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
			const char * pw = mapDownloader.pendingCreate.password.empty() ? nullptr : mapDownloader.pendingCreate.password.c_str();
			world.lobby.CreateGame(
				mapDownloader.pendingCreate.gamename.c_str(),
				mapDownloader.pendingCreate.mapname.c_str(),
				mapDownloader.pendingCreate.maphash,
				pw,
				mapDownloader.pendingCreate.securitylevel,
				mapDownloader.pendingCreate.minlevel,
				mapDownloader.pendingCreate.maxlevel,
				mapDownloader.pendingCreate.maxplayers,
				mapDownloader.pendingCreate.maxteams);
		}else if(us == 3){
			mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
			game.creategameclicked = false;
			DismissProgressModal(ctx);
			ctx.PushScreen(std::make_unique<MessageModal>("Could not upload map"));
		}
		if(world.lobby.creategamestatus == 1){
			world.lobby.creategamestatus = 0;
			LobbyGame * lobbygame = world.lobby.GetGameById(world.lobby.createdgameid);
			if(lobbygame){
				game.JoinGame(*lobbygame, lobbygame->password);
				mapDownloader.LoadMapData(mapDownloader.FindMap(lobbygame->mapname, &lobbygame->maphash).c_str());
				game.currentlobbygameid = lobbygame->id;
			}
		}else if(world.lobby.creategamestatus != 100 && world.lobby.creategamestatus != 0){
			world.lobby.creategamestatus = 0;
			game.creategameclicked = false;
			DismissProgressModal(ctx);
			ctx.PushScreen(std::make_unique<MessageModal>("Could not create game"));
		}
	}

	if(game.gameselectinterface || game.gamecreateinterface){
		// Joining attempt finalisation (success → CONNECTED, failure → IDLE).
		if(game.joininggame){
			if(world.state == World::CONNECTED){
				game.joininggame = false;
			}
			if(world.state == World::IDLE){
				game.joininggame = false;
				DismissProgressModal(ctx);
				ctx.PushScreen(std::make_unique<MessageModal>("Unable to join game"));
			}
		}
		// Spinner text update for the create-game progress modal.
		if(MessageModal * progress = TopAsProgressModal(ctx)){
			std::string text = (mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 1)
				? "Uploading map" : "Creating game";
			int dots = (world.tickcount / 4) % 6;
			if(dots > 3) dots = 6 - dots;
			for(int i = 0; i < dots; i++) text += ".";
			progress->SetText(ctx, text);
		}
		// Auto-dismiss the progress modal once the create flow has settled.
		if(TopAsProgressModal(ctx) && world.lobby.creategamestatus != 100 &&
		   mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 0 &&
		   (world.state == World::CONNECTED || world.state == World::IDLE)){
			ctx.PopScreen();
			game.creategameclicked = false;
		}
		// CONNECTED transition — swap in the GameJoinPanel, refresh chat
		// channel, set the map-name overlay.
		if(world.state == World::CONNECTED && game.lobbyinterface){
			Peer * peer = world.peerlist[world.localpeerid];
			if(peer){
				mapDownloader.mapexistchecked = false;
				mapDownloader.mapjoingeneration.fetch_add(1, std::memory_order_relaxed);
				mapDownloader.mapjoinstate.store(0, std::memory_order_relaxed);
				if(mapDownloader.mapjointhread.joinable()) mapDownloader.mapjointhread.detach();
				world.SetTech(Config::GetInstance().defaulttechchoices[Config::GetInstance().defaultagency]);
				ShowGameJoin(ctx);
				LobbyGame * lobbygame = world.lobby.GetGameById(game.currentlobbygameid);
				if(lobbygame){
					char temp[256];
					ctx.ambienceMixer.GetGameChannelName(*lobbygame, temp);
					strcpy(world.lobby.lastchannel, world.lobby.channel);
					world.lobby.JoinChannel(temp);
					SetMapNameOverlay(world, lobbygame->mapname);
				}
			}
		}
	}

	mapDownloader.ProcessMapDownload();

	// Disconnected-from-game modal: only when in the joined-game surface.
	if(world.state != World::CONNECTED && !TopIsModal(ctx)){
		if(game.gamejoininterface || game.gametechinterface){
			Game * gamePtr = &game;
			ctx.PushScreen(std::make_unique<MessageModal>("Disconnected from game", [gamePtr]() { gamePtr->GoBack(); }));
		}
	}
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	character.Destroy(ctx);
	chat.Destroy(ctx);
	if(gameSelect){ gameSelect->Destroy(ctx); gameSelect.reset(); }
	if(gameCreate){ gameCreate->Destroy(ctx); gameCreate.reset(); }
	if(gameJoin)  { gameJoin->Destroy(ctx);   gameJoin.reset();   }
	if(gameTech)  { gameTech->Destroy(ctx);   gameTech.reset();   }
	if(interfaceId){
		Interface * iface = static_cast<Interface *>(ctx.world.GetObjectFromId(interfaceId));
		if(iface) iface->DestroyInterface(ctx.world);
		interfaceId = 0;
	}
	ctx.game.lobbyinterface = 0;
}

void LobbyScreen::SetMapNameOverlay(World & world, const char * name)
{
	Interface * lobbyiface = static_cast<Interface *>(world.GetObjectFromId(interfaceId));
	if(!lobbyiface) return;
	for(Uint16 childId : lobbyiface->objects){
		Object * object = world.GetObjectFromId(childId);
		if(!object || object->type != ObjectTypes::OVERLAY) continue;
		Overlay * overlay = static_cast<Overlay *>(object);
		if(overlay->uid == 8){
			overlay->text = name;
			overlay->text = overlay->text.substr(0, 25);
		}
	}
}

// Tear down any active right-side panel + matching mirror id on Game.
// Called by every Show* swap helper before building the new panel.
static void TearDownRightPanels(ScreenContext & ctx, Interface * lobbyiface,
                                std::unique_ptr<GameSelectPanel> & gameSelect,
                                std::unique_ptr<GameCreatePanel> & gameCreate,
                                std::unique_ptr<GameJoinPanel>   & gameJoin,
                                std::unique_ptr<GameTechPanel>   & gameTech)
{
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
	if(gameTech){
		Interface * panelIface = (Interface *)ctx.world.GetObjectFromId(gameTech->interfaceId);
		if(panelIface) panelIface->DestroyInterface(ctx.world, lobbyiface);
		gameTech.reset();
		ctx.game.gametechinterface = 0;
		ctx.world.choosingtech = false;
		ctx.game.ShowTeamOverlays(true);
	}
}

void LobbyScreen::ShowGameSelect(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;
	TearDownRightPanels(ctx, lobbyiface, gameSelect, gameCreate, gameJoin, gameTech);
	gameSelect = std::unique_ptr<GameSelectPanel>(new GameSelectPanel(*this));
	gameSelect->Build(ctx, lobbyiface);
}

void LobbyScreen::ShowGameCreate(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;
	TearDownRightPanels(ctx, lobbyiface, gameSelect, gameCreate, gameJoin, gameTech);

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
	TearDownRightPanels(ctx, lobbyiface, gameSelect, gameCreate, gameJoin, gameTech);

	gameJoin = std::unique_ptr<GameJoinPanel>(new GameJoinPanel(*this));
	gameJoin->Build(ctx, lobbyiface);
}

void LobbyScreen::ShowGameTech(ScreenContext & ctx)
{
	Interface * lobbyiface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
	if(!lobbyiface) return;
	TearDownRightPanels(ctx, lobbyiface, gameSelect, gameCreate, gameJoin, gameTech);

	ctx.world.choosingtech = true;
	ctx.game.ShowTeamOverlays(false);
	ctx.world.RequestPeerList();

	gameTech = std::unique_ptr<GameTechPanel>(new GameTechPanel(*this));
	gameTech->Build(ctx, lobbyiface);
	ctx.game.gametechinterface = gameTech->interfaceId;

	lobbyiface->activeobject = gameTech->interfaceId;
	Interface * chatiface = (Interface *)ctx.world.GetObjectFromId(ctx.game.chatinterface);
	if(chatiface){
		chatiface->activeobject = 0;
	}
	ctx.game.currentinterface = interfaceId;
	lobbyiface->ActiveChanged(ctx.world, lobbyiface, false);
}
