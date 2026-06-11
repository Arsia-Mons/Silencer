#include "glyph_fonts.h"

#include <math.h>
#include <string.h>

#include <algorithm>
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
  for (Variant &v : variants_) {
    if (v.face.atlas) {
      SDL_DestroyTexture(v.face.atlas);
      v.face.atlas = nullptr;
    }
    v = Variant{};
  }
  for (auto &kv : string_variants_)
    if (kv.second.texture)
      SDL_DestroyTexture(kv.second.texture);
  string_variants_.clear();
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

bool GlyphFonts::build_color_face(SDL_Renderer *renderer, int face_id,
                                  uint8_t key_r, uint8_t key_g, uint8_t key_b,
                                  const GlyphSrc *glyphs, int count,
                                  const SDL_Color *palette256, float advance,
                                  float line_height, int legacy_w,
                                  int legacy_h) {
  if (!renderer || face_id < 0 || face_id >= kFaceCount || !glyphs ||
      !palette256)
    return false;
  if (count > kGlyphCount)
    count = kGlyphCount;

  Face f{};
  f.advance = advance;
  f.line_height = line_height;

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
      f.gw[i] = 0;
    }
  }
  const int atlas_w = pen > 0 ? pen : 1;
  const int atlas_h = max_h > 0 ? max_h : 1;
  f.atlas_h = atlas_h;

  // Verbatim palettized colors, opaque where index != 0 — origin's DrawText
  // blits the indexed glyph with no alpha blending, so the rendered text IS
  // these exact pixels (the AA lives in the art's color ramp, not in alpha).
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
        const SDL_Color &c = palette256[idx];
        drow[x * 4 + 0] = c.r;
        drow[x * 4 + 1] = c.g;
        drow[x * 4 + 2] = c.b;
        drow[x * 4 + 3] = 255;
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
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
  f.atlas = tex;
  f.loaded = true;

  // Replace an existing registration for this (face, key) or take a free slot.
  Variant *slot = nullptr;
  for (Variant &v : variants_) {
    if (v.used && v.face_id == face_id && v.r == key_r && v.g == key_g &&
        v.b == key_b) {
      slot = &v;
      break;
    }
  }
  if (!slot) {
    for (Variant &v : variants_) {
      if (!v.used) {
        slot = &v;
        break;
      }
    }
  }
  if (!slot) {
    SDL_DestroyTexture(tex);
    return false;
  }
  if (slot->face.atlas)
    SDL_DestroyTexture(slot->face.atlas);
  slot->face = f;
  slot->face_id = static_cast<uint16_t>(face_id);
  slot->r = key_r;
  slot->g = key_g;
  slot->b = key_b;
  slot->used = true;
  // CPU copy for the string-variant bake (straight RGBA, ink opaque).
  slot->pixels = std::move(rgba);
  slot->atlas_w = atlas_w;
  slot->legacy_w = legacy_w > 0 ? legacy_w : 640;
  slot->legacy_h = legacy_h > 0 ? legacy_h : 480;
  return true;
}

bool GlyphFonts::string_variant(SDL_Renderer *renderer, uint16_t face_id,
                                uint8_t r, uint8_t g, uint8_t b,
                                const char *text, int len, float pen_dev_x,
                                float pen_dev_y, float gscale, int out_w,
                                int out_h, StringVariant *out) {
  if (!renderer || !out || !text || len <= 0 || len > 255 || out_w <= 0 ||
      out_h <= 0)
    return false;
  const Variant *var = nullptr;
  for (const Variant &v : variants_) {
    if (v.used && v.face_id == face_id && v.r == r && v.g == g && v.b == b) {
      var = &v;
      break;
    }
  }
  if (!var || var->pixels.empty())
    return false;
  const Face &f = var->face;
  float s =
      std::min(out_w / (float)var->legacy_w, out_h / (float)var->legacy_h);
  if (s < 1.0f)
    s = 1.0f;
  // Only 1:1-virtual text qualifies (glyph native px -> s device px).
  if (fabsf(gscale - s) > 0.02f * s)
    return false;
  // Mirror the SW renderer's dst-rect floor, then recover origin's virtual pen
  // (authoring places the pen within +-1 device px of the golden cell).
  const int px = (int)floorf(pen_dev_x), py = (int)floorf(pen_dev_y);
  const int vx = (int)floorf(px / s + 0.5f);
  const int vy = (int)floorf(py / s + 0.5f);
  const int adv = (int)(f.advance + 0.5f); // origin pen step is integer virtual

  // Virtual-space string footprint (monospace pen; art may overlap the step).
  int w_v = 0;
  for (int i = 0; i < len; ++i) {
    const unsigned char ch = (unsigned char)text[i];
    if (ch < kFirstChar || ch > kLastChar)
      continue;
    const int gi = ch - kFirstChar;
    if (f.gw[gi] > 0) {
      const int e = i * adv + f.gw[gi];
      if (e > w_v)
        w_v = e;
    }
  }
  const int h_v = f.atlas_h;
  if (w_v <= 0 || h_v <= 0)
    return false;

  // Whole-frame magnify placement (origin Render), centered.
  const int vw = std::max(1, (int)(out_w / s));
  const int vh = std::max(1, (int)(out_h / s));
  const int scaled_w = (int)(vw * s + 0.5f);
  const int scaled_h = (int)(vh * s + 0.5f);
  const int off_x = scaled_w < out_w ? (out_w - scaled_w) / 2 : 0;
  const int off_y = scaled_h < out_h ? (out_h - scaled_h) / 2 : 0;
  // The string's device cell [ceil(vx*s), ceil((vx+w_v)*s)) (epsilon guards
  // float ceil at integral products).
  const int x0 = off_x + (int)ceilf(vx * s - 0.001f);
  const int y0 = off_y + (int)ceilf(vy * s - 0.001f);
  const int tw = off_x + (int)ceilf((vx + w_v) * s - 0.001f) - x0;
  const int th = off_y + (int)ceilf((vy + h_v) * s - 0.001f) - y0;
  if (tw <= 0 || th <= 0)
    return false;

  // Memo key: face | color | device-cell phase | string bytes.
  std::string key;
  key.reserve((size_t)len + 8);
  key.push_back((char)(face_id & 0xff));
  key.push_back((char)(face_id >> 8));
  key.push_back((char)r);
  key.push_back((char)g);
  key.push_back((char)b);
  key.push_back((char)(((x0 % 18) + 18) % 18));
  key.push_back((char)(((y0 % 18) + 18) % 18));
  key.append(text, (size_t)len);
  auto it = string_variants_.find(key);
  if (it != string_variants_.end()) {
    *out = it->second;
    out->x = x0; // shared phase, this draw's absolute cell
    out->y = y0;
    return true;
  }
  if ((int)string_variants_.size() >= kStringVariantCap) {
    for (auto &kv : string_variants_)
      if (kv.second.texture)
        SDL_DestroyTexture(kv.second.texture);
    string_variants_.clear();
  }

  // Compose the string in virtual space: origin DrawText — integer pen,
  // transparent-skipping glyph blits, later glyphs over earlier.
  std::vector<uint8_t> vbuf((size_t)w_v * h_v * 4u, 0u);
  for (int i = 0; i < len; ++i) {
    const unsigned char ch = (unsigned char)text[i];
    if (ch < kFirstChar || ch > kLastChar)
      continue;
    const int gi = ch - kFirstChar;
    const int gw = f.gw[gi];
    if (gw <= 0)
      continue;
    const int penv = i * adv;
    for (int yy = 0; yy < h_v; ++yy) {
      const uint8_t *srow =
          var->pixels.data() + ((size_t)yy * var->atlas_w + f.gx[gi]) * 4u;
      uint8_t *drow = vbuf.data() + ((size_t)yy * w_v + penv) * 4u;
      for (int xx = 0; xx < gw && penv + xx < w_v; ++xx) {
        if (srow[xx * 4 + 3] == 0)
          continue;
        memcpy(drow + (size_t)xx * 4, srow + (size_t)xx * 4, 4);
      }
    }
  }

  // Magnify per absolute device pixel: src = int((gx - off)/s) — origin's
  // whole-frame NEAREST chain evaluated at this string's cell.
  std::vector<uint8_t> dbuf((size_t)tw * th * 4u, 0u);
  for (int dy = 0; dy < th; ++dy) {
    const int ly = (int)((y0 + dy - off_y) / s) - vy;
    if (ly < 0 || ly >= h_v)
      continue;
    uint8_t *drow = dbuf.data() + (size_t)dy * tw * 4u;
    const uint8_t *srow = vbuf.data() + (size_t)ly * w_v * 4u;
    for (int dx = 0; dx < tw; ++dx) {
      const int lx = (int)((x0 + dx - off_x) / s) - vx;
      if (lx < 0 || lx >= w_v)
        continue;
      const uint8_t *sp = srow + (size_t)lx * 4u;
      if (sp[3] == 0)
        continue;
      memcpy(drow + (size_t)dx * 4u, sp, 4);
    }
  }

  SDL_Surface *surf = SDL_CreateSurfaceFrom(tw, th, SDL_PIXELFORMAT_RGBA32,
                                            dbuf.data(), tw * 4);
  if (!surf)
    return false;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
  SDL_DestroySurface(surf);
  if (!tex)
    return false;
  // Ink is opaque (straight == premultiplied); drawn 1:1, nearest.
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
  StringVariant sv;
  sv.texture = tex;
  sv.x = x0;
  sv.y = y0;
  sv.w = tw;
  sv.h = th;
  string_variants_.emplace(std::move(key), sv);
  *out = sv;
  return true;
}

const GlyphFonts::Face *GlyphFonts::color_face(uint16_t face_id, uint8_t r,
                                               uint8_t g, uint8_t b) const {
  for (const Variant &v : variants_) {
    if (v.used && v.face_id == face_id && v.r == r && v.g == g && v.b == b)
      return &v.face;
  }
  return nullptr;
}

bool GlyphFonts::any_loaded() const {
  for (const Face &f : faces_)
    if (f.loaded)
      return true;
  return false;
}

} // namespace silencer::cppx_ui
