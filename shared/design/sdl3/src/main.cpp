// Silencer design-system SDL3 hydration — entry point.
//
// Currently scoped to the main menu (see docs/design/screen-main-menu.md).
// Adding more screens means registering more factories below.
//
// Controls:
//   Tab / Enter / Esc   forwarded to the active screen's Interface
//   F                   toggle fullscreen
//   Q                   quit
//
// Capture mode:
//   SILENCER_DUMP_DIR=<dir> ./silencer_design <assets>
//     Renders each screen once and writes a PPM into <dir>, then exits.
//     Bypasses macOS Screen-Recording / Spaces friction so QA can compare
//     against the real client's framebuffer dump.

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

// Sub-palette used by the main menu (clients/silencer/src/game.cpp:494).
constexpr std::size_t kMenuSubPalette = 1;

}  // namespace

int main(int argc, char** argv) {
    using namespace silencer;

    // Default: walk up from build/ to repo's shared/assets/. When running from
    // shared/design/sdl3/build/, this resolves to ../../assets — pass the real
    // path on the command line if the binary is elsewhere.
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

    Palette palette;
    if (!palette.Load(assets_dir + "PALETTE.BIN")) {
        std::fprintf(stderr, "Failed to load palette from %s\n", assets_dir.c_str());
    }
    palette.SetActive(kMenuSubPalette);  // main menu uses sub-palette 1

    SpriteBanks banks;
    if (!banks.LoadIndex(assets_dir)) {
        std::fprintf(stderr, "Failed to load BIN_SPR.DAT from %s\n", assets_dir.c_str());
    }
    // Banks the main menu touches (see docs/design/sprite-banks.md).
    const unsigned wanted_banks[] = {6, 132, 133, 134, 135, 136, 208};
    for (unsigned b : wanted_banks) banks.LoadBank(b);

    SDL_StartTextInput(window);

    std::vector<std::unique_ptr<Screen>> screens;
    screens.push_back(MakeMainMenuScreen());

    int current = 0;

    if (const char* dump_dir = std::getenv("SILENCER_DUMP_DIR")) {
        std::vector<std::uint8_t> dfb(kLogicalW * kLogicalH, 0);
        std::vector<std::uint32_t> drgba(kLogicalW * kLogicalH, 0);
        DrawCtx dctx{dfb.data(), kLogicalW, kLogicalH, &banks, &palette, 0};
        for (auto& s : screens) s->Init(dctx);
        for (std::size_t i = 0; i < screens.size(); ++i) {
            // Tick 120 times so the bank-208 logo reaches its steady-state
            // frame (idx 60). The real client's dump fires after 8 frames in
            // MAINMENU which is mid-fade-in; we bias the hydration toward the
            // hold frame so QA compares like-for-like at the visually
            // canonical logo. Mismatches in animation timing aren't part of
            // the component-fidelity gate.
            for (int t = 0; t < 120; ++t) screens[i]->Tick();
            dctx.state_i = 120;
            Clear(dfb.data(), kLogicalW, kLogicalH, 0);
            screens[i]->Draw(dctx);
            palette.IndexedToRgba(dfb.data(), drgba.data(), dfb.size());
            char path[1024];
            std::snprintf(path, sizeof(path), "%s/screen_%02zu.ppm", dump_dir, i);
            if (FILE* f = std::fopen(path, "wb")) {
                std::fprintf(f, "P6\n%d %d\n255\n", kLogicalW, kLogicalH);
                for (int p = 0; p < kLogicalW * kLogicalH; ++p) {
                    std::uint32_t px = drgba[p];
                    unsigned char rgb[3] = {
                        static_cast<unsigned char>(px & 0xff),
                        static_cast<unsigned char>((px >> 8) & 0xff),
                        static_cast<unsigned char>((px >> 16) & 0xff)};
                    std::fwrite(rgb, 1, 3, f);
                }
                std::fclose(f);
                std::printf("dumped %s (%s)\n", path, screens[i]->Title().c_str());
            }
        }
        SDL_DestroyTexture(tex);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }

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
                    if (kc == SDLK_F) {
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(window, fullscreen);
                    } else if (kc == SDLK_Q) {
                        running = false;
                    } else {
                        screens[current]->OnKey(kc);
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

        Uint64 now = SDL_GetTicks();
        while (now - last_tick_ms >= kTickMs) {
            state_i++;
            screens[current]->Tick();
            last_tick_ms += kTickMs;
        }
        ctx.state_i = state_i;

        screens[current]->OnMouse(mouse, ctx);

        Clear(fb.data(), kLogicalW, kLogicalH, 0);
        screens[current]->Draw(ctx);

        palette.IndexedToRgba(fb.data(), rgba.data(), fb.size());

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
