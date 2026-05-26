#include "lobby_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "message_modal.h"

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

	silencer::client_ui::lobby::CharacterPanelTick(characterState, ctx);
	if(characterState.newCharacterRequested){
		characterState.newCharacterRequested = false;
		ctx.GoToState(GameState::CREATECHARACTER);
		return;
	}
	silencer::client_ui::lobby::ChatPanelTick(chatState, ctx);

	if(!gameCreateActive && !gameJoinActive && !gameTechActive){
		silencer::client_ui::lobby::GameSelectPanelTick(
			gameSelectState, ctx);
	}
	if(gameCreateActive){
		silencer::client_ui::lobby::GameCreatePanelTick(
			gameCreateState, ctx);
	}
	if(gameJoinActive){
		silencer::client_ui::lobby::GameJoinPanelTick(
			gameJoinState, ctx);
	}
	if(gameTechActive){
		silencer::client_ui::lobby::GameTechPanelTick(
			gameTechState, ctx);
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

void LobbyScreen::QueueGoBack()
{
	if(goBackQueued) return;
	goBackQueued = true;
	if(goBack) goBack();
}

bool LobbyScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		if(HandleBack(ctx)) return true;
		QueueGoBack();
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate &&
	   action.id == lobby_controller_detail::kActionGoBack){
		QueueGoBack();
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
	if(silencer::client_ui::lobby::GameSelectPanelHandleUiIntent(gameSelectState, action)){
		if(gameSelectState.createClicked){
			gameSelectState.createClicked = false;
			ShowGameCreate(ctx);
		}
		return true;
	}
	return false;
}
