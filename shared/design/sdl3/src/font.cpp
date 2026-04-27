#include "font.h"

#include <cstdint>
#include <cstring>

namespace silencer {

void DrawText(Framebuffer &fb, int x, int y, const std::string &text, int bank,
              int advance, const SpriteSet &sprites, const Palette &palette,
              int active_sub, int brightness) {
  // Bank 132 uses ioffset 34; 133..136 use 33.
  int ioffset = (bank == 132) ? 34 : 33;

  // Build brightness LUT once per call (only used if brightness != 128).
  std::array<uint8_t, 256> lut{};
  const uint8_t *lut_ptr = nullptr;
  if (brightness != 128) {
    lut = palette.BuildBrightnessLUT(active_sub, brightness);
    lut_ptr = lut.data();
  }

  int xc = 0;
  for (unsigned char ch : text) {
    if (ch == ' ' || ch == 0xA0) {
      xc += advance;
      continue;
    }
    int gi = static_cast<int>(ch) - ioffset;
    if (!sprites.Has(bank, gi)) {
      xc += advance;
      continue;
    }
    const Sprite &g = sprites.Get(bank, gi);
    BlitSprite(fb, g, x + xc, y, lut_ptr);
    xc += advance;
  }
}

}  // namespace silencer
