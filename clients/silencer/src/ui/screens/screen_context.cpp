#include "screen_context.h"

#include "screen.h"
#include "modal.h"
#include <cassert>

ScreenContext::ScreenContext(Game & game_,
                             World & world_,
                             Renderer & renderer_,
                             Lobby & lobby_,
                             KeyMap & keymap_,
                             Updater & updater_)
    : world(world_),
      renderer(renderer_),
      lobby(lobby_),
      keymap(keymap_),
      updater(updater_),
      game(game_)
{
	(void)game;
}

void ScreenContext::GoToState(Uint8) { assert(false && "ScreenContext::GoToState not wired yet"); }
void ScreenContext::GoBack() { assert(false && "ScreenContext::GoBack not wired yet"); }
void ScreenContext::RequestQuit() { assert(false && "ScreenContext::RequestQuit not wired yet"); }
void ScreenContext::PushScreen(std::unique_ptr<Screen>) { assert(false && "ScreenContext::PushScreen not wired yet"); }
void ScreenContext::PopScreen() { assert(false && "ScreenContext::PopScreen not wired yet"); }
void ScreenContext::ReplaceScreen(std::unique_ptr<Screen>) { assert(false && "ScreenContext::ReplaceScreen not wired yet"); }
void ScreenContext::ShowModal(std::unique_ptr<Modal>) { assert(false && "ScreenContext::ShowModal not wired yet"); }
void ScreenContext::ShowMessage(const char *, std::function<void(bool)>) { assert(false && "ScreenContext::ShowMessage not wired yet"); }
