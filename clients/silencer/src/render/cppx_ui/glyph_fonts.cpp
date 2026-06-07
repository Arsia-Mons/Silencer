#include "glyph_fonts.h"

#include <string.h>
#include <vector>

namespace silencer::cppx_ui {

GlyphFonts::~GlyphFonts() { shutdown(); }

void GlyphFonts::shutdown() {
  for (Face &f : faces_) {
    if (f.atlas) {
      SDL_DestroyTexture(f.atlas);
      f.atlas = nullptr;
    }
    f = Face{};
  }
}

bool GlyphFonts::build_face(SDL_Renderer *renderer, int face_id,
                            const GlyphSrc *glyphs, int count,
                            const SDL_Color *palette256, float advance,
                            float line_height) {
  if (!renderer || face_id < 0 || face_id >= kFaceCount || !glyphs ||
      !palette256)
    return false;
  if (count > kGlyphCount)
    count = kGlyphCount;

  // Peak luminance across the face's glyph art, so the brightest pixel maps to
  // full coverage (1.0) and a flat tint reproduces the original color exactly.
  auto lum = [&](uint8_t idx) -> int {
    const SDL_Color &c = palette256[idx];
    int m = c.r > c.g ? c.r : c.g;
    return m > c.b ? m : c.b; // max channel (glyph ramps are single-hue)
  };
  int peak = 1;
  for (int i = 0; i < count; ++i) {
    const GlyphSrc &g = glyphs[i];
    if (!g.indices || g.w <= 0 || g.h <= 0)
      continue;
    const int n = g.w * g.h;
    for (int p = 0; p < n; ++p) {
      const uint8_t idx = g.indices[p];
      if (idx != 0) {
        const int l = lum(idx);
        if (l > peak)
          peak = l;
      }
    }
  }

  Face f{};
  f.advance = advance;
  f.line_height = line_height;

  // Pass 1: lay glyphs out in a row, 1px gutter so nearest-neighbor sampling of
  // one glyph never bleeds into the next. atlas_h = tallest glyph.
  int pen = 0;
  int max_h = 0;
  for (int i = 0; i < count; ++i) {
    const GlyphSrc &g = glyphs[i];
    if (g.indices && g.w > 0 && g.h > 0) {
      f.gx[i] = static_cast<int16_t>(pen);
      f.gw[i] = static_cast<int16_t>(g.w);
      pen += g.w + 1;
      if (g.h > max_h)
        max_h = g.h;
    } else {
      f.gx[i] = 0;
      f.gw[i] = 0; // blank cell (space): advance only, no art
    }
  }
  const int atlas_w = pen > 0 ? pen : 1;
  const int atlas_h = max_h > 0 ? max_h : 1;
  f.atlas_h = atlas_h;

  // Pass 2: bake each glyph as a WHITE coverage mask (premultiplied) into the
  // atlas. Index 0 is transparent (the world renderer's sprite convention);
  // every other index becomes white with alpha = its luminance / peak — the
  // original art's ramp preserved as coverage falloff. The executor tints at
  // draw time (premultiplied white * cov, color-modded by the token color).
  std::vector<uint8_t> rgba(static_cast<size_t>(atlas_w) * atlas_h * 4u, 0);
  for (int i = 0; i < count; ++i) {
    const GlyphSrc &g = glyphs[i];
    if (!g.indices || g.w <= 0 || g.h <= 0 || f.gw[i] == 0)
      continue;
    const int gx = f.gx[i];
    for (int y = 0; y < g.h; ++y) {
      const uint8_t *srow = g.indices + static_cast<size_t>(y) * g.w;
      uint8_t *drow = rgba.data() + (static_cast<size_t>(y) * atlas_w + gx) * 4u;
      for (int x = 0; x < g.w; ++x) {
        const uint8_t idx = srow[x];
        if (idx == 0)
          continue;
        int cov = lum(idx) * 255 / peak;
        if (cov > 255)
          cov = 255;
        const uint8_t c = static_cast<uint8_t>(cov); // premultiplied white*cov
        drow[x * 4 + 0] = c;
        drow[x * 4 + 1] = c;
        drow[x * 4 + 2] = c;
        drow[x * 4 + 3] = c;
      }
    }
  }

  SDL_Surface *surf = SDL_CreateSurfaceFrom(
      atlas_w, atlas_h, SDL_PIXELFORMAT_RGBA32, rgba.data(), atlas_w * 4);
  if (!surf)
    return false;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
  SDL_DestroySurface(surf);
  if (!tex)
    return false;
  // Premultiplied (mask is white premul) + nearest sampling for the chunky
  // upscaled-bitmap look that matches the golden's 640->960 nearest upscale.
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

  if (faces_[face_id].atlas)
    SDL_DestroyTexture(faces_[face_id].atlas);
  f.atlas = tex;
  f.loaded = true;
  faces_[face_id] = f;
  return true;
}

const GlyphFonts::Face *GlyphFonts::face(uint16_t face_id) const {
  if (face_id < kFaceCount && faces_[face_id].loaded)
    return &faces_[face_id];
  if (faces_[0].loaded) // Body fallback
    return &faces_[0];
  return nullptr;
}

bool GlyphFonts::any_loaded() const {
  for (const Face &f : faces_)
    if (f.loaded)
      return true;
  return false;
}

} // namespace silencer::cppx_ui
