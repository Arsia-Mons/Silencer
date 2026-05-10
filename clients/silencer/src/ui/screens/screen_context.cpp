#include "screen_context.h"

#include "game.h"
#include "screen.h"
#include "modal.h"
#include <cassert>

ScreenContext::ScreenContext(Game & game_,
                             World & world_,
                             Renderer & renderer_,
                             Lobby & lobby_,
                             KeyMap & keymap_,
                             Updater & updater_,
                             AmbienceMixer & ambienceMixer_,
                             SDL_Window * & window_,
                             RenderDevice * & renderdevice_)
    : world(world_),
      renderer(renderer_),
      lobby(lobby_),
      keymap(keymap_),
      updater(updater_),
      ambienceMixer(ambienceMixer_),
      window(window_),
      renderdevice(renderdevice_),
      game(game_)
{
}

void ScreenContext::GoToState(Uint8 newState) { game.GoToState(newState); }
void ScreenContext::GoBack() { game.GoBack(); }
void ScreenContext::RequestQuit() { game.quitRequested = true; }
void ScreenContext::PushScreen(std::unique_ptr<Screen> s) { game.PushScreen(std::move(s)); }
void ScreenContext::PopScreen() { game.PopScreen(); }
void ScreenContext::ReplaceScreen(std::unique_ptr<Screen> s) { game.ReplaceScreen(std::move(s)); }
void ScreenContext::ShowModal(std::unique_ptr<Modal>) { assert(false && "ScreenContext::ShowModal not wired yet"); }
void ScreenContext::ShowMessage(const char *, std::function<void(bool)>) { assert(false && "ScreenContext::ShowMessage not wired yet"); }
