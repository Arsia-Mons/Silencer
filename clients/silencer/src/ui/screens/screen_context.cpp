#include "screen_context.h"

#include "game.h"
#include "screen.h"
#include "modal.h"
#include "renderdevice.h"
#include <SDL3/SDL_video.h>
#include <cassert>
#include <cstring>

ScreenContext::ScreenContext(Game & game_,
                             World & world_,
                             Renderer & renderer_,
                             Lobby & lobby_,
                             KeyMap & keymap_,
                             Updater & updater_,
                             AmbienceMixer & ambienceMixer_)
    : world(world_),
      renderer(renderer_),
      lobby(lobby_),
      keymap(keymap_),
      updater(updater_),
      ambienceMixer(ambienceMixer_),
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

void ScreenContext::LoadActiveKeymap() { game.LoadActiveKeymap(); }
void ScreenContext::CycleKeybindPreset() { game.CycleKeybindPreset(); }
void ScreenContext::ForkActiveProfileIfBuiltin() { game.ForkActiveProfileIfBuiltin(); }

void ScreenContext::SetFullscreen(bool on)
{
	if(game.window) SDL_SetWindowFullscreen(game.window, on);
}

void ScreenContext::SetScaleFilter(bool on)
{
	if(game.renderdevice) game.renderdevice->SetScaleFilter(on);
}

const char * ScreenContext::KeyName(SDL_Scancode sc) const
{
	return game.GetKeyName(sc);
}

void ScreenContext::LaunchStage2() { game.LaunchStage2(); }

void ScreenContext::SetLocalUsername(const char * name)
{
	if(!name) return;
	const size_t cap = sizeof(game.localusername);
	strncpy(game.localusername, name, cap - 1);
	game.localusername[cap - 1] = '\0';
}
