#include "palette.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace silencer {

bool Palette::LoadFromFile(const std::string &path) {
  std::FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) {
    std::fprintf(stderr, "palette: cannot open %s\n", path.c_str());
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  long file_size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);

  std::vector<uint8_t> buf(file_size, 0);
  if (file_size > 0) {
    std::fread(buf.data(), 1, file_size, f);
  }
  std::fclose(f);

  // Zero-initialize palettes (already default-initialized to 0).
  //
  // SPEC GAP — docs/design/palette.md claims `offset(s) = 4 + s * (768 + 4)`
  // i.e. a 4-byte file prefix + 11 × [4-byte sub-header + 768-byte colors].
  // That formula gives 8 + s * 772 for the COLORS (skipping each sub's header),
  // total 4 + 11*772 = 8496 bytes — but the file is 8448, and resolving the
  // main-menu sub-palette (1) under `8 + s*772` yields a purple/blue tint
  // instead of the reference's green/red. Empirically, the actual stride is
  // 768 with a single 8-byte prefix at the start: `color_offset(s) = 8 + s*768`.
  // Total = 8 + 11*768 = 8456; the last 8 bytes of sub-palette 10 over-read
  // the file (zero-fill, matching the spec note about the engine's over-read).
  // Under this formula, sub-palette 1 produces the reference's green logo /
  // green chrome / green text / red planet / black starfield exactly.
  for (int s = 0; s < 11; ++s) {
    long color_offset = 8 + static_cast<long>(s) * 768;
    for (int i = 0; i < 256; ++i) {
      uint8_t r = 0, g = 0, b = 0;
      long ridx = color_offset + i * 3 + 0;
      long gidx = color_offset + i * 3 + 1;
      long bidx = color_offset + i * 3 + 2;
      if (ridx < file_size) r = buf[ridx];
      if (gidx < file_size) g = buf[gidx];
      if (bidx < file_size) b = buf[bidx];
      // 6-bit -> 8-bit by left shift 2.
      palettes[s][i][0] = static_cast<uint8_t>(r << 2);
      palettes[s][i][1] = static_cast<uint8_t>(g << 2);
      palettes[s][i][2] = static_cast<uint8_t>(b << 2);
    }
  }
  return true;
}

std::array<uint8_t, 256> Palette::BuildBrightnessLUT(int sub_palette,
                                                    int brightness) const {
  std::array<uint8_t, 256> lut{};
  for (int i = 0; i < 256; ++i) lut[i] = static_cast<uint8_t>(i);

  if (brightness == 128) return lut;

  const auto &pal = palettes[sub_palette];
  for (int i = 2; i < 256; ++i) {
    int r = pal[i][0];
    int g = pal[i][1];
    int b = pal[i][2];
    int tr, tg, tb;
    if (brightness > 128) {
      double t = (brightness - 127) / 128.0;
      tr = static_cast<int>(r + (255 - r) * t);
      tg = static_cast<int>(g + (255 - g) * t);
      tb = static_cast<int>(b + (255 - b) * t);
    } else {
      double t = brightness / 128.0;
      tr = static_cast<int>(r * t);
      tg = static_cast<int>(g * t);
      tb = static_cast<int>(b * t);
    }
    if (tr < 0) tr = 0;
    if (tr > 255) tr = 255;
    if (tg < 0) tg = 0;
    if (tg > 255) tg = 255;
    if (tb < 0) tb = 0;
    if (tb > 255) tb = 255;

    int best_j = i;
    long best_d = 0x7fffffff;
    for (int j = 2; j < 256; ++j) {
      int dr = tr - pal[j][0];
      int dg = tg - pal[j][1];
      int db = tb - pal[j][2];
      long d = static_cast<long>(dr) * dr + static_cast<long>(dg) * dg +
               static_cast<long>(db) * db;
      if (d < best_d) {
        best_d = d;
        best_j = j;
      }
    }
    lut[i] = static_cast<uint8_t>(best_j);
  }
  return lut;
}

}  // namespace silencer
