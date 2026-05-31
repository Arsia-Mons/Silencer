#include "sprite_bake.h"

namespace silencer::cppx_ui {

void bake_indexed_rgba(const uint8_t *indices, int w, int h,
                       const SDL_Color *palette256, uint8_t *out_rgba) {
  const int n = w * h;
  for (int i = 0; i < n; ++i) {
    const uint8_t idx = indices[i];
    uint8_t *o = out_rgba + i * 4;
    if (idx == 0) { // transparent index (world renderer skips pixel==0)
      o[0] = 0;
      o[1] = 0;
      o[2] = 0;
      o[3] = 0;
      continue;
    }
    const SDL_Color &c = palette256[idx];
    // Opaque (alpha 255) => premultiplied RGB == straight RGB.
    o[0] = c.r;
    o[1] = c.g;
    o[2] = c.b;
    o[3] = 255;
  }
}

} // namespace silencer::cppx_ui
