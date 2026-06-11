#include "sprite_bake.h"

#include <algorithm>

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

void bake_backdrop_rgba(const uint8_t *indices, int sw, int sh,
                        const SDL_Color *palette256, bool stretch,
                        int legacy_w, int legacy_h, int dw, int dh,
                        uint8_t *out_rgba) {
  float s = std::min(dw / (float)legacy_w, dh / (float)legacy_h);
  if (s < 1.0f)
    s = 1.0f;
  const int vw = std::max(1, (int)(dw / s));
  const int vh = std::max(1, (int)(dh / s));
  // Hop 1 (sprite -> virtual canvas), origin DrawImage arithmetic.
  const float sx_scale = (float)vw / sw;
  const float sy_scale = (float)vh / sh;
  const float cover = std::max(sx_scale, sy_scale);
  const int draw_w = stretch ? vw : std::max(1, (int)(sw * cover + 0.5f));
  const int draw_h = stretch ? vh : std::max(1, (int)(sh * cover + 0.5f));
  const int ox = (vw - draw_w) / 2;
  const int oy = (vh - draw_h) / 2;
  // Hop 2 (virtual -> device), origin whole-frame magnify, centered.
  const int scaled_w = (int)(vw * s + 0.5f);
  const int scaled_h = (int)(vh * s + 0.5f);
  const int off_x = scaled_w < dw ? (dw - scaled_w) / 2 : 0;
  const int off_y = scaled_h < dh ? (dh - scaled_h) / 2 : 0;
  const int out_w = std::min(scaled_w, dw - off_x);
  const int out_h = std::min(scaled_h, dh - off_y);
  for (int dy = 0; dy < out_h; ++dy) {
    int vy = std::min((int)(dy / s), vh - 1);
    int syi = stretch ? (int)(vy / sy_scale) : (int)((vy - oy) / cover);
    if (syi < 0 || syi >= sh)
      continue;
    uint8_t *orow = out_rgba + ((size_t)(off_y + dy) * dw + off_x) * 4;
    const uint8_t *srow = indices + (size_t)syi * sw;
    for (int dx = 0; dx < out_w; ++dx) {
      int vx = std::min((int)(dx / s), vw - 1);
      int sxi = stretch ? (int)(vx / sx_scale) : (int)((vx - ox) / cover);
      if (sxi < 0 || sxi >= sw)
        continue;
      const uint8_t idx = srow[sxi];
      if (idx == 0)
        continue;
      const SDL_Color &c = palette256[idx];
      uint8_t *o = orow + (size_t)dx * 4;
      o[0] = c.r;
      o[1] = c.g;
      o[2] = c.b;
      o[3] = 255;
    }
  }
}

} // namespace silencer::cppx_ui
