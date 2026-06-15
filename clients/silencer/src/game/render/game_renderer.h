#ifndef GAME_RENDERER_H
#define GAME_RENDERER_H

#include "renderdevice.h"
#include "surface.h"
#include <SDL3/SDL.h>
#include <memory>

namespace silencer::cppx_ui { class UiDemoOverlay; }

// Virtual resolution that the game was originally designed for.
// Used by the renderer, UI pipeline, and game loop to compute scale factors.
inline constexpr int kLegacyRenderWidth  = 640;
inline constexpr int kLegacyRenderHeight = 480;

class Game;
struct SDL_Window;

class GameRenderer
{
public:
// Direction of the transition palette fade. The fade phase clock is the same
// in both directions (16 phases); the direction decides whether the screen
// dims to black (Out) or rises from black (In). Decoupled from
// GameState::FADEOUT so Tier-1 overlay transitions (options/pause) can run a
// full out->in fade without owning a gameplay state change.
enum class FadeDir : Uint8 { In, Out };

explicit GameRenderer(Game & game);
~GameRenderer();

bool Setup(SDL_Window ** outWindow);
static Uint32 TimerCallback(void * userdata, SDL_TimerID timerID, Uint32 interval);
void Present();
bool ResizeRenderSurface(int width, int height);
bool ResizeRenderSurfacePixels(int width, int height);
bool SyncRenderSurfaceToWindowPixels();
// SIL-240 menu reflow fix: the last window pixel size, refreshed only at window
// creation and on real resize events (never per render frame). The menu's
// logical canvas (game_ui_pipeline) derives its aspect from THIS so it stays
// pinned across the in-game 640x480 surface pin (map load/unload) and across any
// transient/late SDL_GetWindowSizeInPixels reading on a render frame. Falls back
// to the surface size when no window has been sized yet (headless).
void RefreshWindowPixelSize();
bool WindowPixelSize(int & width, int & height) const {
	if(windowPixelW_ < 1 || windowPixelH_ < 1) return false;
	width = windowPixelW_;
	height = windowPixelH_;
	return true;
}
void SetColors(SDL_Color * colors);
void RestartPaletteFade(FadeDir dir = FadeDir::In);
FadeDir GetFadeDir() const { return fadeDir_; }
bool PaletteFadeFinished() const;
Uint8 PaletteFadePhaseFromClock() const;
void ApplyPaletteFade(bool fadeOut);
// SIL-219: the global opacity [0,1] the cppx UI layer should composite at so
// it fades in/out in lockstep with the world's transition palette fade. 1.0
// at rest (no fade); mirrors the brightness fraction ApplyPaletteFade applies
// to the world during a FADEOUT transition and the subsequent fade-in.
float UiFadeAlpha() const;
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
int windowPixelW_ = 0; // last window pixel size (SIL-240 menu canvas source)
int windowPixelH_ = 0;
Uint8 fade_i;
Uint64 fadeStartMs;
FadeDir fadeDir_ = FadeDir::In;
// SIL-237: the fade alpha applied at the LAST cppx UI upload. The dirty-skip in
// Present() only skips the upload when BOTH the IR is unchanged AND this alpha
// is unchanged (the fade is applied at upload time, not in the IR). -1 forces
// the first upload. Sentinel < 0 == "no prior upload".
float lastUiFadeAlpha_ = -1.0f;
std::unique_ptr<silencer::cppx_ui::UiDemoOverlay> cppxDemo; // SIL-11 flag-gated demo overlay
};

#endif
