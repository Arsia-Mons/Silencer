#ifndef GAME_RENDERER_H
#define GAME_RENDERER_H

#include "renderdevice.h"
#include "surface.h"
#include <SDL3/SDL.h>

// Virtual resolution that the game was originally designed for.
// Used by the renderer, UI pipeline, and game loop to compute scale factors.
inline constexpr int kLegacyRenderWidth  = 640;
inline constexpr int kLegacyRenderHeight = 480;

class Game;
struct SDL_Window;

class GameRenderer
{
public:
explicit GameRenderer(Game & game);

bool Setup(SDL_Window ** outWindow);
static Uint32 TimerCallback(void * userdata, SDL_TimerID timerID, Uint32 interval);
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
