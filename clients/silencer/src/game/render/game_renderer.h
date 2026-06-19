#ifndef GAME_RENDERER_H
#define GAME_RENDERER_H

#include "renderdevice.h"
#include "surface.h"
#include <SDL3/SDL.h>
#include <memory>

namespace silencer::cppx_ui {
class UiDemoOverlay;
}

// Virtual resolution the game was originally designed for.
inline constexpr int kLegacyRenderWidth = 640;
inline constexpr int kLegacyRenderHeight = 480;

class Game;
struct SDL_Window;

class GameRenderer {
public:
    // Direction of the transition palette fade (16 phases either way): dim to
    // black (Out) or rise from black (In). Decoupled from GameState::FADEOUT so
    // Tier-1 overlay transitions can run a full out->in fade without a state change.
    enum class FadeDir : Uint8 { In,
                                 Out };

    explicit GameRenderer(Game &game);
    ~GameRenderer();

    bool Setup(SDL_Window **outWindow);
    static Uint32 TimerCallback(void *userdata, SDL_TimerID timerID, Uint32 interval);
    void Present();
    bool ResizeRenderSurface(int width, int height);
    bool ResizeRenderSurfacePixels(int width, int height);
    bool SyncRenderSurfaceToWindowPixels();
    // Refresh the cached window pixel size. Called only at window creation and on
    // real resize events (never per render frame) so the menu canvas's aspect
    // source never lags a transient/late SDL_GetWindowSizeInPixels poll.
    void RefreshWindowPixelSize();
    bool WindowPixelSize(int &width, int &height) const {
        if (windowPixelW_ < 1 || windowPixelH_ < 1)
            return false;
        width = windowPixelW_;
        height = windowPixelH_;
        return true;
    }
    void SetColors(SDL_Color *colors);
    void RestartPaletteFade(FadeDir dir = FadeDir::In);
    FadeDir GetFadeDir() const { return fadeDir_; }
    bool PaletteFadeFinished() const;
    Uint8 PaletteFadePhaseFromClock() const;
    void ApplyPaletteFade(bool fadeOut);
    // Global opacity [0,1] for the cppx UI layer: mirrors ApplyPaletteFade's
    // brightness fraction so UI fades in lockstep with the world. 1.0 at rest.
    float UiFadeAlpha() const;
    float LegacyUiAnimationStepSeconds() const;
    void LoadProgressCallback(int progress, int totalprogressitems);

    Surface &GetScreenBuffer() { return screenbuffer; }
    const Surface &GetScreenBuffer() const { return screenbuffer; }
    const SDL_Color *GetPaletteColors() const { return palettecolors; }
    SDL_Window *GetWindow() const { return window; }
    SDL_Window *&WindowRef() { return window; }
    RenderDevice *GetRenderDevice() const { return renderdevice; }
    RenderDevice *&RenderDeviceRef() { return renderdevice; }
    Uint8 &FadePhaseRef() { return fade_i; }

private:
    Game &game;
    RenderDevice *renderdevice;
    Surface screenbuffer;
    SDL_Color palettecolors[256];
    SDL_Window *window;
    int windowPixelW_ = 0; // last window pixel size (menu canvas source)
    int windowPixelH_ = 0;
    Uint8 fade_i;
    Uint64 fadeStartMs;
    FadeDir fadeDir_ = FadeDir::In;
    // Fade alpha at the last cppx UI upload; the Present() dirty-skip keys off it
    // (fade is applied at upload time, not in the IR). < 0 == no prior upload.
    float lastUiFadeAlpha_ = -1.0f;
    std::unique_ptr<silencer::cppx_ui::UiDemoOverlay> cppxDemo; // flag-gated demo overlay
};

#endif
