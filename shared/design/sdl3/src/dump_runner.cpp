#include "dump_runner.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <filesystem>
#include <vector>

namespace silencer {

namespace {

bool WritePPM(const std::string &path, const Framebuffer &fb,
              const Palette &palette, int active_sub) {
  std::FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "ppm: cannot open %s for writing\n", path.c_str());
    return false;
  }
  std::fprintf(f, "P6\n%d %d\n255\n", Framebuffer::W, Framebuffer::H);
  std::vector<uint8_t> rgb(static_cast<size_t>(Framebuffer::W) * Framebuffer::H * 3);
  const auto &pal = palette.palettes[active_sub];
  for (int i = 0; i < Framebuffer::W * Framebuffer::H; ++i) {
    uint8_t idx = fb.px[i];
    rgb[i * 3 + 0] = pal[idx][0];
    rgb[i * 3 + 1] = pal[idx][1];
    rgb[i * 3 + 2] = pal[idx][2];
  }
  std::fwrite(rgb.data(), 1, rgb.size(), f);
  std::fclose(f);
  return true;
}

}  // namespace

int RunScreenDump(const ScreenSpec &spec, const std::string &assets_dir,
                  const std::string &dump_dir) {
  if (!SDL_Init(0)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  Palette palette;
  if (!palette.LoadFromFile(assets_dir + "/PALETTE.BIN")) {
    SDL_Quit();
    return 1;
  }

  SpriteSet sprites;
  if (!sprites.Load(assets_dir, spec.banks)) {
    SDL_Quit();
    return 1;
  }

  Framebuffer fb;
  fb.px.fill(spec.background_index);

  spec.compose(fb, sprites, palette, spec.active_sub_palette);

  std::filesystem::create_directories(dump_dir);
  std::string out = dump_dir + "/screen_00.ppm";
  bool ok = WritePPM(out, fb, palette, spec.active_sub_palette);
  std::fprintf(stderr, "wrote %s (%s)\n", out.c_str(), spec.name);

  SDL_Quit();
  return ok ? 0 : 1;
}

}  // namespace silencer
