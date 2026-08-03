#ifndef LAUNCHER_BACKGROUNDS_H
#define LAUNCHER_BACKGROUNDS_H

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace launcher {

// One menu backdrop decoded from the game's sprite banks: 8-bit palette
// indices (index 0 = transparent) + the palette page it is authored against.
// Ready for PipelineHost::bake_chrome_sprite.
struct Background {
  int w = 0;
  int h = 0;
  std::vector<uint8_t> indices;
  SDL_Color palette[256] = {};
};

// Decodes the curated set of screen-sized backdrops from
// `<assets_dir>/bin_spr/SPR_*.BIN` (+ BIN_SPR.DAT / PALETTE.BIN): the bank
// 0-4 parallax level backdrops (assembled from their 64px tile grids, junk
// rows below the art cropped) and the bank 007/208 menu screens. Backgrounds
// that fail to load are skipped; an empty vector means no assets found.
std::vector<Background> load_backgrounds(const std::string &assets_dir);

} // namespace launcher

#endif
