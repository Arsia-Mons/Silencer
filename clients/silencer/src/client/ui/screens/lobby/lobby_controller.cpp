#include "lobby_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "lobbygame.h"
#include "serializer.h"
#include "message_modal.h"
#include "peer.h"

#include <cstring>
#include <memory>
#include <string>

namespace lobby_controller_detail {

constexpr const char * kActionGoBack = "lobby.go_back";

MessageModal * TopAsProgressModal(ScreenContext & ctx)
{
	Screen * top = ctx.TopScreen();
	if(!top) return nullptr;
	MessageModal * m = dynamic_cast<MessageModal *>(top);
	return (m && m->IsProgress()) ? m : nullptr;
}

bool TopIsModal(ScreenContext & ctx)
{
	Screen * top = ctx.TopScreen();
	return top && top->IsOverlay();
}

void DismissProgressModal(ScreenContext & ctx)
{
	if(TopAsProgressModal(ctx)) ctx.PopScreen();
}

}  // namespace lobby_controller_detail

void LobbyScreen::Tick(ScreenContext & ctx)
{
	// Lobby disconnect → bounce back to the connect screen.
	if(ctx.HandleLobbyDisconnect()) return;

	World & world = ctx.world;

	// Chrome Go Back — flag was set by a typed button intent on the previous
	// frame. Consume it before pumping anything else.
	if(goBackClicked){
		goBackClicked = false;
		if(ctx.GoBack()) return;
	}

	silencer::client_ui::lobby::CharacterPanelTick(characterState, ctx);
	if(characterState.newCharacterRequested){
		characterState.newCharacterRequested = false;
		ctx.GoToState(GameState::CREATECHARACTER);
		return;
	}
	silencer::client_ui::lobby::ChatPanelTick(chatState, ctx);

	if(!gameCreateActive && !gameJoinActive && !gameTechActive){
		silencer::client_ui::lobby::GameSelectPanelTick(
			gameSelectState, ctx, *this);
	}
	if(gameCreateActive){
		silencer::client_ui::lobby::GameCreatePanelTick(
			gameCreateState, ctx.world, ctx, *this);
	}
	if(gameJoinActive){
		silencer::client_ui::lobby::GameJoinPanelTick(
			gameJoinState, ctx, *this);
	}
	if(gameTechActive){
		silencer::client_ui::lobby::GameTechPanelTick(
			gameTechState, ctx.world, ctx, *this);
	}

	// Pre-CONNECTED surfaces (gameselect / gamecreate) — join finalisation,
	// progress-modal spinner update, auto-dismiss, CONNECTED→GameJoin
	// transition.
	if(!gameJoinActive && !gameTechActive){
		ScreenContext::JoiningGameResult joiningGameResult =
			ctx.ConsumeJoiningGameResult();
		if(joiningGameResult == ScreenContext::JoiningGameResult::Failed){
			lobby_controller_detail::DismissProgressModal(ctx);
			ctx.ShowMessage("Unable to join game");
		}
		if(MessageModal * progress = lobby_controller_detail::TopAsProgressModal(ctx)){
			progress->SetText(ctx, ctx.CreateGameProgressText());
		}
		if(lobby_controller_detail::TopAsProgressModal(ctx) &&
		   ctx.ShouldDismissCreateGameProgress()){
			ctx.PopScreen();
			ctx.SetCreateGamePending(false);
		}
		if(ctx.LobbyNetworkConnected()){
			if(ctx.BeginConnectedLobbyGame()){
				ShowGameJoin(ctx);
				std::string mapName = ctx.JoinCurrentLobbyGameChannel();
				if(!mapName.empty()) SetMapNameOverlay(mapName.c_str());
			}
		}
	}

	ctx.PumpMapDownload();

	// Disconnect-from-game modal — fires on the joined-game surface
	// (gameJoinActive || gameTechActive) when the world drops out of
	// CONNECTED.
	if(ctx.JoinedGameDisconnected() && !lobby_controller_detail::TopIsModal(ctx)){
		if(gameJoinActive || gameTechActive){
			ctx.ShowMessage("Disconnected from game", [&ctx]() { ctx.GoBack(); });
		}
	}
}

bool LobbyScreen::HandleBack(ScreenContext & ctx)
{
	if(gameJoinActive || gameTechActive){
		ctx.LeaveJoinedGame();
		SetMapNameOverlay("");
		ShowGameSelect(ctx);
		return true;
	}
	if(gameCreateActive){
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
	if(silencer::client_ui::lobby::CharacterPanelHandleUiIntent(characterState, ctx, action)){
		return true;
	}
	if(silencer::client_ui::lobby::ChatPanelHandleUiIntent(chatState, ctx, action)){
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

// Friend-of-World helpers — these are member methods because the remaining
// lobby panels reach into World private state through the LobbyScreen friend
// grant. Kept alongside the controller (Tick / HandleUiIntent) since the
// panel ticks call them.

void LobbyScreen::SeedHostGameInfo(World & world, LobbyGame & lg)
{
	Serializer data;
	lg.Serialize(Serializer::WRITE, data);
	world.gameinfo.Serialize(Serializer::READ, data);
}

bool LobbyScreen::JoinPanelReadyBlocked(World & world) const
{
	if(world.gameplaystate != World::INLOBBY) return false;
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
