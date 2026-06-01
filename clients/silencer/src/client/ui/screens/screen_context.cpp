#include "screen_context.h"

#include "client/ui/screens/main_menu/main_menu_screen.h"
#include "client/ui/screens/mission_summary/mission_summary_screen.h"
#ifdef SILENCER_HAVE_LOBBY_UI
#include "client/ui/screens/lobby/lobby_screen.h"
#endif
#include "game.h"
#include "game_state.h"
#include "renderer.h"
#include "screen.h"
#include "modal.h"
#include "message_modal.h"
#include "surface.h"

#include <cassert>
#include <memory>

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

void ScreenContext::StartTutorialGame() { game.GoToState(GameState::SINGLEPLAYERGAME); }
void ScreenContext::RequestQuit() { game.quitRequested = true; }
void ScreenContext::LeaveJoinedGame() { game.LeaveJoinedGame(); }
void ScreenContext::UnloadGame() { game.gameSession.UnloadGame(); }
void ScreenContext::PushScreen(std::unique_ptr<Screen> s) { game.PushScreen(std::move(s)); }
void ScreenContext::PopScreen() { game.PopScreen(); }
void ScreenContext::ResetToScreen(std::unique_ptr<Screen> s) { game.ResetToScreen(std::move(s)); }
void ScreenContext::ResetGameToUiScreen(std::unique_ptr<Screen> screen) {
	game.state = GameState::NONE;
	game.nextstate = GameState::NONE;
	game.stateisnew = false;
	game.nextstateprocessed = true;
	game.ResetToScreen(std::move(screen));
}

void ScreenContext::ShowMainMenu() {
	ResetGameToUiScreen(std::make_unique<MainMenuScreen>());
}

void ScreenContext::ShowLobby() {
#ifdef SILENCER_HAVE_LOBBY_UI
	ResetGameToUiScreen(std::make_unique<LobbyScreen>());
#else
	ResetGameToUiScreen(std::make_unique<MainMenuScreen>());
#endif
}

void ScreenContext::ShowMissionSummary() {
	ResetGameToUiScreen(std::make_unique<MissionSummaryScreen>());
}

void ScreenContext::ShowModal(std::unique_ptr<Modal> m) {
	game.PushScreen(std::unique_ptr<Screen>(static_cast<Screen *>(m.release())));
}

void ScreenContext::ShowMessage(const char * msg, std::function<void()> onClose) {
	game.PushScreen(std::make_unique<MessageModal>(msg ? msg : "", std::move(onClose)));
}

void ScreenContext::ResetPresentation(int paletteIdx) {
	renderer.palette.SetPalette(paletteIdx);
	game.GetScreenBuffer().Clear(0);
	game.gameRenderer.SetColors(renderer.palette.GetColors());
}

void ScreenContext::PlayMenuMusicIfReady() {
	if(game.gameSession.AmbienceMixerRef().FadedIn()){
		game.gameSession.AmbienceMixerRef().PlayMusic(world.resources.menumusic);
	}
}
