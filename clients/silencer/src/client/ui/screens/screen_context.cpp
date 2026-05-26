#include "screen_context.h"

#include "game.h"
#include "renderer.h"
#include "screen.h"
#include "modal.h"
#include "message_modal.h"
#include "surface.h"
#include "runtime/UiInteractionRegistry.h"
#include "lobby.h"

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
      world(world_),
      renderer(renderer_),
      lobby(lobby_),
      keymap(keymap_),
      updater(updater_),
      ambienceMixer(ambienceMixer_),
      mapDownloader(mapDownloader_),
      window(window_),
      renderdevice(renderdevice_)
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
void ScreenContext::SetCurrentLobbyGameId(Uint32 gameId) { game.currentlobbygameid = gameId; }
LobbyGame * ScreenContext::CurrentLobbyGame() const { return world.lobby.GetGameById(game.currentlobbygameid); }
void ScreenContext::JoinGame(LobbyGame & lobbyGame, char * password) { game.JoinGame(lobbyGame, password); }
void ScreenContext::SpectateGame(LobbyGame & lobbyGame, char * password) { game.SpectateGame(lobbyGame, password); }
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
