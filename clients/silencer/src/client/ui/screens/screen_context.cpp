#include "screen_context.h"

#include "game.h"
#include "renderdevice.h"
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

void ScreenContext::ResetPresentation(int paletteIdx) {
	game.ResetPresentationPalette(paletteIdx);
}

void ScreenContext::CenterPresentationCamera() {
	renderer.camera.SetPosition(320, 240);
}

void ScreenContext::BeginLobbyPanelBorderBlur(int virtualWidth,
                                              int virtualHeight,
                                              float uiScale) {
	if(!renderdevice) return;
	renderdevice->BeginLobbyPanelBorderBlur(virtualWidth, virtualHeight, uiScale);
}

void ScreenContext::AddLobbyPanelBorderBlurRect(int x, int y, int w, int h) {
	if(!renderdevice || w <= 0 || h <= 0) return;
	renderdevice->AddLobbyPanelBorderBlurRect({ x, y, w, h });
}
