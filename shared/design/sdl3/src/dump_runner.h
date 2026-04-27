#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "palette.h"
#include "sprite.h"

namespace silencer {

// Per-screen configuration consumed by RunScreenDump. Plain data; no SDL
// types so the registry can live in a static array.
//
// Lifecycle (all shared work moved here from the per-screen RunDump*
// functions):
//
//   SDL_Init(0) -> Palette::LoadFromFile -> SpriteSet::Load(banks)
//     -> fb.px.fill(background_index)
//     -> compose(fb, sprites, palette, active_sub_palette)
//     -> WritePPM(dump_dir/screen_00.ppm) -> SDL_Quit
//
// `name` doubles as the CLI selector (SILENCER_DUMP_SCREEN=<name>).
struct ScreenSpec {
  const char *name;
  std::vector<int> banks;
  int active_sub_palette;
  uint8_t background_index;
  void (*compose)(Framebuffer &fb, const SpriteSet &sprites,
                  const Palette &palette, int active_sub);
};

// Runs the spec end-to-end and writes screen_00.ppm into dump_dir. Returns
// 0 on success, 1 on failure (stderr already explained why).
int RunScreenDump(const ScreenSpec &spec, const std::string &assets_dir,
                  const std::string &dump_dir);

}  // namespace silencer
