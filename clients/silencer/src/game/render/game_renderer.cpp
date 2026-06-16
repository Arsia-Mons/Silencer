#include "render/game_renderer.h"

#include "game.h"
#include "config.h"
#include "gasloader.h"
#include "sdl3gpubackend.h"
#include "tuibackend.h"
#include "game/ui/session_phase.h"
#include "render/cppx_ui/ui_demo.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

GameRenderer::GameRenderer(Game &g)
    : game(g), renderdevice(nullptr), screenbuffer(640, 480), window(nullptr), fade_i(0), fadeStartMs(0) {
    std::memset(palettecolors, 0, sizeof(palettecolors));
}

GameRenderer::~GameRenderer() = default;

bool GameRenderer::Setup(SDL_Window **outWindow) {
    if (outWindow) {
        window = *outWindow;
    }
    if (game.tui) {
        TUIBackend *backend = new TUIBackend();
        if (!backend->Init(nullptr)) {
            delete backend;
            return false;
        }
        renderdevice = backend;
        renderdevice->SetScaleFilter(false);
    } else {
        SDL3GPUBackend *backend = new SDL3GPUBackend();
        if (!backend->Init(window)) {
            delete backend;
            return false;
        }
        renderdevice = backend;
        renderdevice->SetScaleFilter(Config::GetInstance().scalefilter);
    }
    if (outWindow) {
        *outWindow = window;
    }
    return true;
}

bool GameRenderer::ResizeRenderSurfacePixels(int width, int height) {
    if (width < 1 || height < 1)
        return false;
    if (screenbuffer.w == width && screenbuffer.h == height)
        return true;
    screenbuffer.Resize(width, height, 0);
    return true;
}

void GameRenderer::RefreshWindowPixelSize() {
    if (!window)
        return;
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height) || width < 1 || height < 1) {
        SDL_GetWindowSize(window, &width, &height);
    }
    if (width > 0 && height > 0) {
        windowPixelW_ = width;
        windowPixelH_ = height;
    }
}

bool GameRenderer::SyncRenderSurfaceToWindowPixels() {
    // A resize/pixel-size event fired (or init) — this is the single point that
    // re-reads the window size, so the cached size the menu canvas keys off stays
    // authoritative and never lags a transient render-frame poll.
    RefreshWindowPixelSize();
    if (game.world.map.loaded) {
        return ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
    }
    if (windowPixelW_ < 1 || windowPixelH_ < 1)
        return false;
    return ResizeRenderSurfacePixels(windowPixelW_, windowPixelH_);
}

bool GameRenderer::ResizeRenderSurface(int width, int height) {
    if (width < 1 || height < 1)
        return false;
    if (window) {
        SDL_SetWindowSize(window, width, height);
        if (game.world.map.loaded) {
            return ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
        }
        return SyncRenderSurfaceToWindowPixels();
    }
    if (game.world.map.loaded) {
        return ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
    }
    return ResizeRenderSurfacePixels(width, height);
}

float GameRenderer::UiFadeAlpha() const {
    using namespace GameState;
    // The cppx UI layer must fade with the world ONLY during a real screen
    // transition (the FADEOUT palette fade and the fade-in that follows it). At
    // in-match rest the world palette is driven by ambience, not the transition
    // fade, so the HUD must stay full — but we cannot gate that on map.loaded: it
    // stays true for the WHOLE FADEOUT leaving a match (UnloadGame runs a frame
    // late, in the MAINMENU stateisnew branch) and for the first menu frame after
    // the switch. That left the HUD + "standby" text at full brightness while the
    // world dimmed to black on exit, and flashed the first menu frame bright before
    // the fade-in took over. Gate on the projected session phase instead: an
    // in-match phase that is NOT mid-FADEOUT is ambience/overlay brightness (stay
    // full); everything else mirrors the world fade. Ambience never enters FADEOUT
    // and never moves fade_i, so the HUD still does not dim with ambience.
    const Uint8 st = game.GetState();
    const int phase = static_cast<int>(fade_i);
    const bool fading = phase < 16;
    // A real state transition (FADEOUT) always dims out; a Tier-1 overlay fade
    // dims out when its direction says so (fade_i is moving and dir == Out).
    const bool fadingOut = (st == FADEOUT) || (fading && fadeDir_ == FadeDir::Out);
    // In-match HUD stays at full brightness EXCEPT while a transition fades OUT
    // (leaving the match, or an in-match overlay dimming to black). Ambience
    // never moves fade_i, so it never dims the HUD through this path.
    if (!fadingOut) {
        using ::client::ui::SessionPhase;
        const SessionPhase ph = silencer::game_ui::project_session_phase(st, game.fadefromstate);
        if (ph == SessionPhase::InMatch || ph == SessionPhase::SinglePlayer || ph == SessionPhase::PostMatch) {
            return 1.0f;
        }
    }
    if (!fading)
        return 1.0f; // at rest
    // Mirror ApplyPaletteFade's brightness exactly so UI and world fade together.
    int brightness;
    if (fadingOut) {
        // Fading OUT: full at phase 0 -> black at phase 15.
        int p = phase > 15 ? 15 : phase;
        brightness = (15 - p) * 8;
    } else {
        // Fading IN: dark at phase 0 -> full at phase >= 15.
        if (phase >= 15)
            return 1.0f;
        brightness = phase * 8;
    }
    // Palette::Brightness maps brightness<128 to a linear (brightness/128) dim.
    float frac = static_cast<float>(brightness) / 128.0f;
    if (frac < 0.0f)
        frac = 0.0f;
    if (frac > 1.0f)
        frac = 1.0f;
    return frac;
}

void GameRenderer::Present() {
    if (renderdevice) {
        renderdevice->UploadFrame(screenbuffer.pixels.data(), screenbuffer.w, screenbuffer.h);
#ifdef SILENCER_CPPX_FONT_DIR
        // SIL-11 end-to-end demo: when SILENCER_CPPX_UI_DEMO is set, render a cppx
        // nine-slice button + TTF text and hand it to the device's UI composite pass.
        if (window && std::getenv("SILENCER_CPPX_UI_DEMO")) {
            int pw = 0, ph = 0;
            if (SDL_GetWindowSizeInPixels(window, &pw, &ph) && pw > 0 && ph > 0) {
                if (!cppxDemo)
                    cppxDemo = std::make_unique<silencer::cppx_ui::UiDemoOverlay>();
                if (cppxDemo->ensure(pw, ph, SILENCER_CPPX_FONT_DIR)) {
                    int uw = 0, uh = 0;
                    const uint8_t *rgba = cppxDemo->render(&uw, &uh);
                    if (rgba)
                        renderdevice->UploadUiFrame(rgba, uw, uh);
                }
            }
        }
#endif
        // SIL-14: when the golden cppx render path is active, composite its
        // premultiplied-RGBA frame over the world through the UI composite pass.
        {
            // SIL-219: fade the cppx UI layer in lockstep with the FADEOUT palette fade so
            // the UI fades out/in with the world on the transitions that already fade it.
            const float uiFadeAlpha = UiFadeAlpha();
            // SIL-240: GPU geometry path — submit the program built this frame. The fade is
            // a GPU multiply at composite time (no per-pixel CPU dim, no full-window upload),
            // so fades and minimap-hover re-render cheaply every frame.
            if (const silencer::cppx_ui::GpuUiProgram *prog = game.gameUiPipeline.CppxUiProgram()) {
                renderdevice->SubmitUiFrame(*prog, uiFadeAlpha);
                lastUiFadeAlpha_ = uiFadeAlpha;
            } else {
                int uw = 0;
                int uh = 0;
                const uint8_t *uirgba = game.gameUiPipeline.CppxUiFrame(uw, uh);
                if (uirgba && uw > 0 && uh > 0) {
                    // SIL-237: skip the GPU UI upload when the pipeline rastered nothing new (the
                    // IR was byte-identical) AND the fade alpha is unchanged. The backend retains
                    // the last-uploaded ui_tex and re-composites it every Present, so the menu
                    // stays on screen without re-uploading a static frame. The fade alpha is
                    // applied at upload time (not in the IR), so a fade in progress MUST re-upload
                    // even when the IR is identical, or the fade would freeze.
                    const bool uiUnchanged =
                        game.gameUiPipeline.CppxUiUnchanged() && uiFadeAlpha == lastUiFadeAlpha_;
                    if (!uiUnchanged) {
                        renderdevice->UploadUiFrame(uirgba, uw, uh, uiFadeAlpha);
                        lastUiFadeAlpha_ = uiFadeAlpha;
                    }
                } else {
                    // No cppx frame this present (host not ready): nothing to composite. Reset the
                    // fade sentinel so the next real frame always re-uploads (matches the original
                    // no-op-when-null behavior, just tracking the upload state for the skip).
                    lastUiFadeAlpha_ = -1.0f;
                }
            }
        }
        renderdevice->Present();
    }
}

void GameRenderer::LoadProgressCallback(int progress, int totalprogressitems) {
    if (game.world.dedicatedserver.active) {
        return;
    }
    game.HandleSDLEvents();
    if (SDL_GetTicks() - game.lasttick >= 100) {
        int width = std::min(500, screenbuffer.w - 32);
        int widthp = (float(progress) / totalprogressitems) * width;
        int barx = (screenbuffer.w - width) / 2;
        int bary = (screenbuffer.h - 32) / 2;
        game.renderer.DrawFilledRectangle(&screenbuffer, barx, bary, barx + width, bary + 32, 101);
        if (widthp > 0) {
            for (int c = 0; c < 13; c++) {
                int x0 = barx + (c * widthp) / 13;
                int x1 = barx + ((c + 1) * widthp) / 13;
                if (x1 > x0)
                    game.renderer.DrawFilledRectangle(&screenbuffer, x0, bary, x1, bary + 32, 101 + c);
            }
        }
        Present();
        game.lasttick = SDL_GetTicks();
    }
}

void GameRenderer::SetColors(SDL_Color *colors) {
    std::memcpy(palettecolors, colors, 256 * sizeof(SDL_Color));
    if (renderdevice) {
        renderdevice->SetPalette(colors, 256);
    }
}

void GameRenderer::RestartPaletteFade(FadeDir dir) {
    fadeStartMs = SDL_GetTicks();
    fade_i = 0;
    fadeDir_ = dir;
}

float GameRenderer::LegacyUiAnimationStepSeconds() const {
    const int hz = GASLoader::Get().gameengine.ticksPerSecond > 0
                       ? GASLoader::Get().gameengine.ticksPerSecond
                       : 24;
    return 1.0f / static_cast<float>(hz);
}

Uint8 GameRenderer::PaletteFadePhaseFromClock() const {
    if (fadeStartMs == 0)
        return fade_i;
    Uint64 now = SDL_GetTicks();
    float elapsedSeconds = 0.0f;
    if (now >= fadeStartMs) {
        elapsedSeconds = static_cast<float>(now - fadeStartMs) / 1000.0f;
    }
    int phase = static_cast<int>(elapsedSeconds / LegacyUiAnimationStepSeconds());
    if (phase < 0)
        phase = 0;
    if (phase > 16)
        phase = 16;
    return static_cast<Uint8>(phase);
}

bool GameRenderer::PaletteFadeFinished() const {
    return PaletteFadePhaseFromClock() >= 16;
}

void GameRenderer::ApplyPaletteFade(bool fadeOut) {
    fade_i = PaletteFadePhaseFromClock();
    int phase = fade_i;
    if (phase > 15)
        phase = 15;
    if (fadeOut) {
        SDL_Color *fadedpalette =
            game.renderer.palette.CopyWithBrightness(game.renderer.palette.GetColors(), (15 - phase) * 8);
        SetColors(fadedpalette);
        return;
    }
    if (phase >= 15) {
        SetColors(game.renderer.palette.GetColors());
        return;
    }
    SDL_Color *fadedpalette =
        game.renderer.palette.CopyWithBrightness(game.renderer.palette.GetColors(), phase * 8);
    SetColors(fadedpalette);
}

Uint32 GameRenderer::TimerCallback(void *userdata, SDL_TimerID, Uint32) {
    Game *g = static_cast<Game *>(userdata);
    g->updatetitle = true;
    g->fps = g->frames;
    return 1000;
}
