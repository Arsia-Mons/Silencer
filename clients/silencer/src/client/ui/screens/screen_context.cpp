#include "screen_context.h"

#include "game.h"
#include "renderer.h"
#include "screen.h"
#include "modal.h"
#include "message_modal.h"
#include "surface.h"
#include "runtime/UiInteractionRegistry.h"
#include "lobby.h"
#include "lobbygame.h"
#include "renderdevice.h"

#include <SDL3/SDL_video.h>

#include <cassert>

ScreenContext::ScreenContext(Game & game_,
                             World & world_,
                             Renderer & renderer_,
                             Lobby & lobby_,
                             KeyMap & keymap_,
                             Updater & updater_,
                             AmbienceMixer & ambienceMixer_,
                             MapDownloader & mapDownloader_,
                             SDL_Window * & window_,
                             RenderDevice * & renderdevice_)
    : game(game_),
      renderer(renderer_),
      window(window_),
      renderdevice(renderdevice_),
      world(world_),
      lobby(lobby_),
      keymap(keymap_),
      updater(updater_),
      ambienceMixer(ambienceMixer_),
      mapDownloader(mapDownloader_)
{
}

void ScreenContext::GoToState(Uint8 newState) { game.GoToState(newState); }
bool ScreenContext::GoBack() { return game.GoBack(); }
void ScreenContext::RequestQuit() { game.quitRequested = true; }
void ScreenContext::LeaveJoinedGame() { game.LeaveJoinedGame(); }
bool ScreenContext::IsJoiningGame() const { return game.joininggame; }
void ScreenContext::SetJoiningGame(bool joining) { game.joininggame = joining; }
bool ScreenContext::IsCreateGamePending() const { return game.creategameclicked; }
void ScreenContext::SetCreateGamePending(bool pending) { game.creategameclicked = pending; }
LobbyGame * ScreenContext::CurrentLobbyGame() const { return world.lobby.GetGameById(game.currentlobbygameid); }
void ScreenContext::JoinLobbyGame(LobbyGame & lobbyGame, char * password) {
	game.currentlobbygameid = lobbyGame.id;
	game.JoinGame(lobbyGame, password);
}
void ScreenContext::SpectateLobbyGame(LobbyGame & lobbyGame, char * password) {
	game.currentlobbygameid = lobbyGame.id;
	game.SpectateGame(lobbyGame, password);
}
SDL_GamepadType ScreenContext::CurrentGamepadType() const {
	SDL_Gamepad * pad = game.GetGamepad();
	return pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
}
bool ScreenContext::PushScreen(std::unique_ptr<Screen> s) { return game.PushScreen(std::move(s)); }
bool ScreenContext::PopScreen() { return game.PopScreen(); }
bool ScreenContext::ReplaceScreen(std::unique_ptr<Screen> s) { return game.ReplaceScreen(std::move(s)); }
Screen * ScreenContext::TopScreen() const { return game.GetTopScreen(); }
bool ScreenContext::ShowModal(std::unique_ptr<Modal> m) {
	return game.PushScreen(std::unique_ptr<Screen>(static_cast<Screen *>(m.release())));
}

bool ScreenContext::ShowMessage(const char * msg, std::function<void()> onClose) {
	return game.PushScreen(std::make_unique<MessageModal>(msg ? msg : "", std::move(onClose)));
}

void ScreenContext::ClearUiFocus() {
	game.UiInteractions().ClearFocus();
}

void ScreenContext::ResetPresentation(int paletteIdx) {
	renderer.palette.SetPalette(paletteIdx);
	game.GetScreenBuffer().Clear(0);
	game.gameRenderer.SetColors(renderer.palette.GetColors());
}

void ScreenContext::ResetMenuPresentation(int paletteIdx) {
	ResetPresentation(paletteIdx);
	renderer.camera.SetPosition(320, 240);
}

bool ScreenContext::UiBlinkVisible() const {
	return (renderer.GetHudAnimationPhase() % 32) < 16;
}

void ScreenContext::SetWindowFullscreen(bool fullscreen) {
	if(window) SDL_SetWindowFullscreen(window, fullscreen);
}

void ScreenContext::SetScaleFilter(bool enabled) {
	if(renderdevice) renderdevice->SetScaleFilter(enabled);
}

void ScreenContext::BeginLobbyPanelBorderBlur(int width, int height, float uiScale) {
	if(renderdevice) renderdevice->BeginLobbyPanelBorderBlur(width, height, uiScale);
}

void ScreenContext::AddLobbyPanelBorderBlurRect(int x, int y, int w, int h) {
	if(!renderdevice || w <= 0 || h <= 0) return;
	renderdevice->AddLobbyPanelBorderBlurRect(SDL_Rect{ x, y, w, h });
}
