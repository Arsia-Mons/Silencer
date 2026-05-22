#include "lobby_screen.h"

#include "screen_context.h"
#include "game.h"
#include "game_state.h"
#include "world.h"
#include "lobby.h"
#include "lobbygame.h"
#include "serializer.h"
#include "config.h"
#include "ambience_mixer.h"
#include "map_downloader.h"
#include "message_modal.h"
#include "peer.h"

#include <cstring>
#include <memory>
#include <string>

namespace lobby_controller_detail {

constexpr const char * kActionGoBack = "lobby.go_back";

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

}  // namespace lobby_controller_detail

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

	// Chrome Go Back — flag was set by a typed button intent on the previous
	// frame. Consume it before pumping anything else.
	if(goBackClicked){
		goBackClicked = false;
		if(game.GoBack()) return;
	}

	silencer::client_ui::lobby::CharacterPanelTick(characterState, ctx.world);
	if(characterState.newCharacterRequested){
		characterState.newCharacterRequested = false;
		ctx.GoToState(GameState::CREATECHARACTER);
		return;
	}
	silencer::client_ui::lobby::ChatPanelTick(chatState, ctx.world);

	if(!gameCreateActive && !gameJoinActive && !gameTechActive){
		silencer::client_ui::lobby::GameSelectPanelTick(
			gameSelectState, ctx.world, ctx, *this);
	}
	if(gameCreateActive){
		silencer::client_ui::lobby::GameCreatePanelTick(
			gameCreateState, ctx.world, ctx, *this);
	}
	if(gameJoinActive){
		silencer::client_ui::lobby::GameJoinPanelTick(
			gameJoinState, ctx.world, ctx, *this);
	}
	if(gameTechActive){
		silencer::client_ui::lobby::GameTechPanelTick(
			gameTechState, ctx.world, ctx, *this);
	}

	MapDownloader & mapDownloader = ctx.mapDownloader;

	// Pre-CONNECTED surfaces (gameselect / gamecreate) — join finalisation,
	// progress-modal spinner update, auto-dismiss, CONNECTED→GameJoin
	// transition.
	if(!gameJoinActive && !gameTechActive){
		if(game.joininggame){
			if(world.network.state == World::CONNECTED){
				game.joininggame = false;
			}
			if(world.network.state == World::IDLE){
				game.joininggame = false;
				lobby_controller_detail::DismissProgressModal(ctx);
				ctx.ShowMessage("Unable to join game");
			}
		}
		if(MessageModal * progress = lobby_controller_detail::TopAsProgressModal(ctx)){
			std::string text = (mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 1)
				? "Uploading map" : "Creating game";
			int dots = (world.tickcount / 4) % 6;
			if(dots > 3) dots = 6 - dots;
			for(int i = 0; i < dots; i++) text += ".";
			progress->SetText(ctx, text);
		}
		if(lobby_controller_detail::TopAsProgressModal(ctx) && world.lobby.creategamestatus != 100 &&
		   mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 0 &&
		   (world.network.state == World::CONNECTED || world.network.state == World::IDLE)){
			ctx.PopScreen();
			game.creategameclicked = false;
		}
		if(world.network.state == World::CONNECTED){
			Peer * peer = world.peers.peerlist[world.peers.localpeerid];
			if(peer){
				mapDownloader.mapexistchecked = false;
				mapDownloader.mapjoingeneration.fetch_add(1, std::memory_order_relaxed);
				mapDownloader.mapjoinstate.store(0, std::memory_order_relaxed);
				if(mapDownloader.mapjointhread.joinable()) mapDownloader.mapjointhread.detach();
				const Uint8 agency = world.lobby.GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
				world.SetTech(Config::GetInstance().defaulttechchoices[agency]);
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

	// Disconnect-from-game modal — fires on the joined-game surface
	// (gameJoinActive || gameTechActive) when the world drops out of
	// CONNECTED.
	if(world.network.state != World::CONNECTED && !lobby_controller_detail::TopIsModal(ctx)){
		if(gameJoinActive || gameTechActive){
			Game * gamePtr = &game;
			ctx.ShowMessage("Disconnected from game", [gamePtr]() { gamePtr->GoBack(); });
		}
	}
}

bool LobbyScreen::HandleBack(ScreenContext & ctx)
{
	if(gameJoinActive || gameTechActive){
		ctx.LeaveJoinedGame();
		SetMapNameOverlay(ctx.world, "");
		ShowGameSelect(ctx);
		return true;
	}
	if(gameCreateActive){
		ctx.world.lobby.gamesprocessed = false;
		ShowGameSelect(ctx);
		return true;
	}
	return false;
}

bool LobbyScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		if(HandleBack(ctx)) return true;
		goBackClicked = true;
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate &&
	   action.id == lobby_controller_detail::kActionGoBack){
		goBackClicked = true;
		return true;
	}
	if(silencer::client_ui::lobby::CharacterPanelHandleUiIntent(characterState, action)){
		return true;
	}
	if(silencer::client_ui::lobby::ChatPanelHandleUiIntent(chatState, ctx.world, action)){
		return true;
	}
	if(gameCreateActive){
		return silencer::client_ui::lobby::GameCreatePanelHandleUiIntent(gameCreateState, action);
	}
	if(gameJoinActive){
		return silencer::client_ui::lobby::GameJoinPanelHandleUiIntent(gameJoinState, action);
	}
	if(gameTechActive){
		return silencer::client_ui::lobby::GameTechPanelHandleUiIntent(gameTechState, action);
	}
	return silencer::client_ui::lobby::GameSelectPanelHandleUiIntent(gameSelectState, action);
}

// Friend-of-World helpers — these are member methods because the lobby
// panels reach into World private state through the LobbyScreen friend
// grant. Kept alongside the controller (Tick / HandleUiIntent) since the
// panel ticks call them.

void LobbyScreen::SeedHostGameInfo(World & world, LobbyGame & lg)
{
	Serializer data;
	lg.Serialize(Serializer::WRITE, data);
	world.gameinfo.Serialize(Serializer::READ, data);
}

bool LobbyScreen::JoinPanelInLobby(World & world) const
{
	return world.gameplaystate == World::INLOBBY;
}

bool LobbyScreen::JoinPanelReadyBlocked(World & world) const
{
	Peer * localpeer = world.peers.peerlist[world.peers.localpeerid];
	return localpeer && localpeer->ishost && !world.AllPeersDownloadedMap();
}

void LobbyScreen::JoinPanelSendReady(World & world)
{
	Peer * localpeer = world.peers.peerlist[world.peers.localpeerid];
	bool ishost = localpeer && localpeer->ishost;
	if(!ishost || world.AllPeersDownloadedMap()){
		world.SendReady();
	}
}

void LobbyScreen::JoinPanelChangeTeam(World & world)
{
	world.ChangeTeam();
}

Uint8 LobbyScreen::TechPanelLocalPeerId(World & world) const
{
	return world.peers.localpeerid;
}

Peer * LobbyScreen::TechPanelPeer(World & world, Uint8 peerid) const
{
	return world.peers.peerlist[peerid];
}

void LobbyScreen::TechPanelRequestPeerList(World & world)
{
	world.RequestPeerList();
}

void LobbyScreen::TechPanelSetTech(World & world, Uint32 techchoices)
{
	world.SetTech(techchoices);
}
