#include "screen_context.h"

#include "game.h"
#include "renderer.h"

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

void ScreenContext::ResetPresentation(int paletteIdx) {
	renderer.palette.SetPalette(paletteIdx);
	game.screenbuffer.Clear(0);
	game.SetColors(renderer.palette.GetColors());
}
