// Silencer main-menu hydration entry point.
//
// Run modes:
//   - SILENCER_DUMP_DIR set: tick until the bank-208 logo settles at
//     idx 60, render once into a 640x480 indexed framebuffer, write
//     screen_00.ppm (P6) into the directory, exit.
//   - Otherwise: open an SDL3 window and present continuously.

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "palette.h"
#include "screens/main_menu.h"
#include "sprite.h"

namespace fs = std::filesystem;

namespace {

constexpr int kFbW = 640;
constexpr int kFbH = 480;

bool WritePpm(const std::string &path,
              const uint8_t *fb,
              const silencer::Palette &palette) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "ppm: open failed: %s\n", path.c_str());
        return false;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", kFbW, kFbH);
    std::vector<uint8_t> rgb(kFbW * kFbH * 3);
    for (int i = 0; i < kFbW * kFbH; ++i) {
        uint8_t idx = fb[i];
        const auto &c = palette.Active(idx);
        rgb[i * 3 + 0] = c.r;
        rgb[i * 3 + 1] = c.g;
        rgb[i * 3 + 2] = c.b;
    }
    std::fwrite(rgb.data(), 1, rgb.size(), f);
    std::fclose(f);
    return true;
}

std::string ResolveAssetsDir(int argc, char **argv) {
    if (argc >= 2) {
        return argv[1];
    }
    // Default: ../../../assets/ relative to the executable.
    const char *base = SDL_GetBasePath();
    std::string s = base ? base : "./";
    fs::path p = fs::path(s) / ".." / ".." / ".." / "assets";
    std::error_code ec;
    fs::path canon = fs::weakly_canonical(p, ec);
    if (!ec) return canon.string();
    return p.string();
}

}  // namespace

int main(int argc, char **argv) {
    const char *dump_dir = std::getenv("SILENCER_DUMP_DIR");
    bool dump_mode = (dump_dir != nullptr && dump_dir[0] != '\0');

    // SDL_Init is required to use SDL_GetBasePath etc.; in dump mode
    // we still init video so the API behaves consistently, but we
    // never create a window.
    Uint32 flags = SDL_INIT_VIDEO;
    if (!SDL_Init(flags)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    std::string assets = ResolveAssetsDir(argc, argv);
    std::fprintf(stderr, "assets dir: %s\n", assets.c_str());

    silencer::Palette palette;
    if (!palette.Load(assets + "/PALETTE.BIN")) {
        SDL_Quit();
        return 1;
    }
    // MAINMENU activates sub-palette 1.
    palette.SetActive(1);

    silencer::Sprites sprites;
    if (!sprites.LoadIndex(assets)) {
        SDL_Quit();
        return 1;
    }
    // Banks needed for the main menu: 6 (background + buttons),
    // 133 (version font), 135 (button-label font), 208 (logo).
    for (int bank : {6, 133, 135, 208}) {
        if (!sprites.LoadBank(assets, bank)) {
            std::fprintf(stderr, "warning: bank %d failed to load\n", bank);
        }
    }

    silencer::MainMenu menu;
    // Spec gap: docs/design/screen-main-menu.md uses "v00026" in its
    // example but doesn't pin the actual string. The most recent
    // committed version per repo CLAUDE.md is "00028", so we use that.
    menu.Build("00028");

    // Dump mode: tick until the bank-208 logo reaches steady state
    // (idx 60). Per widget-overlay.md, that happens at state_i == 60.
    // We tick the interface 60 times so the logo's res_index settles
    // at 60 on the next Draw call.
    if (dump_mode) {
        for (int t = 0; t < 60; ++t) {
            menu.Tick();
        }

        std::vector<uint8_t> fb(kFbW * kFbH, 0);
        menu.Draw(fb.data(), kFbW, kFbH, sprites, palette);

        std::error_code ec;
        fs::create_directories(dump_dir, ec);
        std::string out_path = std::string(dump_dir) + "/screen_00.ppm";
        bool ok = WritePpm(out_path, fb.data(), palette);
        SDL_Quit();
        return ok ? 0 : 1;
    }

    // Interactive mode: open a window, present at 24 Hz.
    SDL_Window *window = SDL_CreateWindow("Silencer (design hydration)",
                                          kFbW, kFbH, 0);
    if (!window) {
        std::fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_Texture *tex = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, kFbW, kFbH);

    std::vector<uint8_t> fb(kFbW * kFbH, 0);
    std::vector<uint8_t> rgba(kFbW * kFbH * 4, 0);
    bool quit = false;
    Uint64 last_tick = SDL_GetTicks();
    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) quit = true;
            if (ev.type == SDL_EVENT_KEY_DOWN &&
                ev.key.key == SDLK_ESCAPE) {
                quit = true;
            }
        }
        Uint64 now = SDL_GetTicks();
        while (now - last_tick >= 42) {
            menu.Tick();
            last_tick += 42;
        }
        std::fill(fb.begin(), fb.end(), uint8_t{0});
        menu.Draw(fb.data(), kFbW, kFbH, sprites, palette);
        for (int i = 0; i < kFbW * kFbH; ++i) {
            const auto &c = palette.Active(fb[i]);
            rgba[i * 4 + 0] = c.r;
            rgba[i * 4 + 1] = c.g;
            rgba[i * 4 + 2] = c.b;
            rgba[i * 4 + 3] = 255;
        }
        SDL_UpdateTexture(tex, nullptr, rgba.data(), kFbW * 4);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, tex, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
