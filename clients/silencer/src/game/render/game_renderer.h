#ifndef GAME_RENDERER_H
#define GAME_RENDERER_H

#include "renderdevice.h"
#include "surface.h"
#include <SDL3/SDL.h>

class Game;
struct SDL_Window;

class GameRenderer
{
public:
explicit GameRenderer(Game & game);

bool Setup(SDL_Window ** outWindow);
void Present();
bool ResizeRenderSurface(int width, int height);
bool ResizeRenderSurfacePixels(int width, int height);
bool SyncRenderSurfaceToWindowPixels();
void SetColors(SDL_Color * colors);
void RestartPaletteFade();
bool PaletteFadeFinished() const;
Uint8 PaletteFadePhaseFromClock() const;
void ApplyPaletteFade(bool fadeOut);
float LegacyUiAnimationStepSeconds() const;
void LoadProgressCallback(int progress, int totalprogressitems);

Surface & GetScreenBuffer() { return screenbuffer; }
const Surface & GetScreenBuffer() const { return screenbuffer; }
const SDL_Color * GetPaletteColors() const { return palettecolors; }
SDL_Window * GetWindow() const { return window; }
SDL_Window * & WindowRef() { return window; }
RenderDevice * GetRenderDevice() const { return renderdevice; }
RenderDevice * & RenderDeviceRef() { return renderdevice; }
Uint8 & FadePhaseRef() { return fade_i; }

private:
Game & game;
RenderDevice * renderdevice;
Surface screenbuffer;
SDL_Color palettecolors[256];
SDL_Window * window;
Uint8 fade_i;
Uint64 fadeStartMs;
};

#endif
