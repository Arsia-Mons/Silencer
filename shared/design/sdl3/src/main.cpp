// Silencer design-system SDL3 hydration — entry point + demo navigator.
//
// Controls:
//   Left / Right arrow  — previous / next demo screen
//   1..9, 0             — jump to numbered screen (0 = 10th)
//   Tab / Enter / Esc   — forwarded to the active screen's Interface
//   F                   — toggle fullscreen
//   Q / Esc-twice       — quit

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "font.h"
#include "palette.h"
#include "screens/screen.h"
#include "sprite.h"
#include "widgets/primitives.h"

namespace {

constexpr int kLogicalW = 640;
constexpr int kLogicalH = 480;

}  // namespace

int main(int argc, char** argv) {
    using namespace silencer;

    // Default: walk up from build/ to repo's shared/assets/. When running from
    // shared/design/sdl3/build/, this resolves to shared/design/assets — pass
    // the real path on the command line if the binary is elsewhere.
    std::string assets_dir = "../../../assets/";
    if (argc >= 2) assets_dir = argv[1];
    if (!assets_dir.empty() && assets_dir.back() != '/') assets_dir += '/';

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Silencer Design — SDL3 hydration", kLogicalW, kLogicalH, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    // Logical 640x480 letterboxed.
    SDL_SetRenderLogicalPresentation(renderer, kLogicalW, kLogicalH,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STREAMING, kLogicalW, kLogicalH);
    if (!tex) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Load assets.
    Palette palette;
    if (!palette.Load(assets_dir + "PALETTE.BIN")) {
        std::fprintf(stderr, "Failed to load palette from %s\n", assets_dir.c_str());
    }
    SpriteBanks banks;
    if (!banks.LoadIndex(assets_dir)) {
        std::fprintf(stderr, "Failed to load BIN_SPR.DAT from %s\n", assets_dir.c_str());
    }
    // Eagerly load the banks the demo touches.
    const unsigned wanted_banks[] = {
        6, 7, 40, 54, 56, 57, 58, 94, 95, 102, 103, 132, 133, 134, 135, 136,
        153, 171, 177, 178, 181, 188, 208, 222};
    for (unsigned b : wanted_banks) banks.LoadBank(b);

    SDL_StartTextInput(window);

    // Build screens.
    std::vector<std::unique_ptr<Screen>> screens;
    screens.push_back(MakePaletteScreen());
    screens.push_back(MakeTypographyScreen());
    screens.push_back(MakeButtonsScreen());
    screens.push_back(MakeInputsScreen());
    screens.push_back(MakeSelectBoxScreen());
    screens.push_back(MakeOverlayScreen());
    screens.push_back(MakePanelScreen());
    screens.push_back(MakeModalScreen());
    screens.push_back(MakeLoadingScreen());
    screens.push_back(MakeHudScreen());
    screens.push_back(MakeMinimapScreen());
    screens.push_back(MakeMainMenuScreen());
    screens.push_back(MakeLobbyScreen());
    screens.push_back(MakeBuyMenuScreen());

    int current = 0;

    // 8-bit indexed framebuffer + RGBA scratch.
    std::vector<std::uint8_t> fb(kLogicalW * kLogicalH, 0);
    std::vector<std::uint32_t> rgba(kLogicalW * kLogicalH, 0);

    DrawCtx ctx{fb.data(), kLogicalW, kLogicalH, &banks, &palette, 0};
    for (auto& s : screens) s->Init(ctx);

    bool running = true;
    bool fullscreen = false;
    Uint64 last_tick_ms = SDL_GetTicks();
    constexpr Uint64 kTickMs = 42;
    std::uint32_t state_i = 0;

    MouseState mouse{};
    bool mouse_was_down = false;

    while (running) {
        // Drain events.
        SDL_Event e;
        mouse.clicked = false;
        mouse.wheel = 0;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN: {
                    int kc = e.key.key;
                    if (kc == SDLK_LEFT) {
                        current = (current - 1 + (int)screens.size()) % (int)screens.size();
                    } else if (kc == SDLK_RIGHT) {
                        current = (current + 1) % (int)screens.size();
                    } else if (kc == SDLK_F) {
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(window, fullscreen);
                    } else if (kc == SDLK_Q) {
                        running = false;
                    } else {
                        // Forward number keys 1..9, 0
                        if (kc >= SDLK_1 && kc <= SDLK_9) {
                            int n = kc - SDLK_1;
                            if (n < (int)screens.size()) current = n;
                        } else if (kc == SDLK_0) {
                            if (10 <= (int)screens.size()) current = 9;
                        } else {
                            screens[current]->OnKey(kc);
                        }
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION: {
                    float lx, ly;
                    SDL_RenderCoordinatesFromWindow(renderer, e.motion.x, e.motion.y, &lx, &ly);
                    mouse.x = (int)lx;
                    mouse.y = (int)ly;
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                    float lx, ly;
                    SDL_RenderCoordinatesFromWindow(renderer, e.button.x, e.button.y, &lx, &ly);
                    mouse.x = (int)lx;
                    mouse.y = (int)ly;
                    mouse.down = true;
                    if (!mouse_was_down) mouse.clicked = true;
                    mouse_was_down = true;
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    mouse.down = false;
                    mouse_was_down = false;
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    mouse.wheel = (e.wheel.y > 0) ? 1 : (e.wheel.y < 0 ? -1 : 0);
                    break;
                case SDL_EVENT_TEXT_INPUT:
                    screens[current]->OnTextInput(e.text.text);
                    break;
                default:
                    break;
            }
        }

        // Tick the simulation at ~24 Hz.
        Uint64 now = SDL_GetTicks();
        while (now - last_tick_ms >= kTickMs) {
            state_i++;
            screens[current]->Tick();
            last_tick_ms += kTickMs;
        }
        ctx.state_i = state_i;

        // Forward mouse input each frame to current screen.
        screens[current]->OnMouse(mouse, ctx);

        // Render.
        Clear(fb.data(), kLogicalW, kLogicalH, 0);
        screens[current]->Draw(ctx);

        // Footer / navigator label.
        char footer[128];
        std::snprintf(footer, sizeof(footer), "[%d/%zu] %s   <-/-> change   F=fullscreen   Q=quit",
                      current + 1, screens.size(), screens[current]->Title().c_str());
        DrawTextOpts fopt;
        fopt.bank = 133;
        fopt.width = 6;
        fopt.brightness = 144;
        DrawText(fb.data(), kLogicalW, kLogicalH, 4, kLogicalH - 12, footer, fopt, banks, palette);

        // Indexed -> RGBA.
        palette.IndexedToRgba(fb.data(), rgba.data(), fb.size());

        // Push to texture and present.
        SDL_UpdateTexture(tex, nullptr, rgba.data(), kLogicalW * 4);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, tex, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_StopTextInput(window);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
