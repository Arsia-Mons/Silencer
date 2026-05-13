#include "screen_context.h"

#include "game.h"
#include "renderer.h"
#include "screen.h"
#include "modal.h"
#include "message_modal.h"
#include "surface.h"
#include "clay/clay.h"
#include "clay_ui_compositor.h"

#include <SDL3/SDL_mouse.h>

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
void ScreenContext::GoBack() { game.GoBack(); }
void ScreenContext::RequestQuit() { game.quitRequested = true; }
void ScreenContext::LeaveJoinedGame() { game.LeaveJoinedGame(); }
void ScreenContext::PushScreen(std::unique_ptr<Screen> s) { game.PushScreen(std::move(s)); }
void ScreenContext::PopScreen() { game.PopScreen(); }
void ScreenContext::ReplaceScreen(std::unique_ptr<Screen> s) { game.ReplaceScreen(std::move(s)); }
void ScreenContext::ShowModal(std::unique_ptr<Modal> m) {
	game.PushScreen(std::unique_ptr<Screen>(static_cast<Screen *>(m.release())));
}

void ScreenContext::ShowMessage(const char * msg, std::function<void()> onClose) {
	game.PushScreen(std::make_unique<MessageModal>(msg ? msg : "", std::move(onClose)));
}

void ScreenContext::ResetPresentation(int paletteIdx) {
	renderer.palette.SetPalette(paletteIdx);
	game.screenbuffer.Clear(0);
	game.SetColors(renderer.palette.GetColors());
}

void ScreenContext::BeginClayFrame(Surface & surface) {
	silencer::clay_bridge::EnsureInitialized(surface.w, surface.h);

	float mx = 0.f;
	float my = 0.f;
	Uint32 buttons = SDL_GetMouseState(&mx, &my);
	bool down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
	Clay_SetPointerState({ mx, my }, down);
}
