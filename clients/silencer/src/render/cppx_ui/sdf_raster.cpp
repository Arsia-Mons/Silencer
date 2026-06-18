#include "sdf_raster.h"

#include <math.h>
#include <stdlib.h>
#include <vector>

namespace silencer::cppx_ui {

namespace {

inline float clampf(float x, float lo, float hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

// Signed distance to an axis-aligned rounded rect centered at the origin, with
// half-extents (bx,by) and corner radius r (Inigo Quilez's sdRoundBox). <0
// inside, >0 outside, all in device pixels.
inline float sd_round_rect(float px, float py, float bx, float by, float r) {
  const float qx = fabsf(px) - bx + r;
  const float qy = fabsf(py) - by + r;
  const float ax = qx > 0.f ? qx : 0.f;
  const float ay = qy > 0.f ? qy : 0.f;
  const float outside = sqrtf(ax * ax + ay * ay);
  const float inside = fminf(fmaxf(qx, qy), 0.f);
  return outside + inside - r;
}

constexpr int kMaxMaskDim = 4096; // defensive cap; bigger -> caller falls back

// Wrap a freshly-filled premultiplied RGBA32 buffer as a 1:1 (nearest) texture
// blended in premultiplied space. Takes ownership of nothing; copies into VRAM.
SDL_Texture *upload_mask(SDL_Renderer *r, const std::vector<uint8_t> &buf, int w,
                         int h) {
  SDL_Surface *surf = SDL_CreateSurfaceFrom(
      w, h, SDL_PIXELFORMAT_RGBA32, const_cast<uint8_t *>(buf.data()), w * 4);
  if (!surf)
    return nullptr;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
  SDL_DestroySurface(surf);
  if (!tex)
    return nullptr;
  // Blitted 1:1 at its native size, so NEAREST keeps the analytic ramp exact.
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
  return tex;
}

// Build a fill coverage mask: white, alpha = SDF coverage of the rounded rect.
// The rect occupies the central (w-2)x(h-2) region; the 1px pad carries the ramp.
SDL_Texture *build_fill_mask(SDL_Renderer *r, int w, int h, float r_px) {
  if (w <= 0 || h <= 0 || w > kMaxMaskDim || h > kMaxMaskDim)
    return nullptr;
  const float half_x = (w - 2) * 0.5f;
  const float half_y = (h - 2) * 0.5f;
  const float cx = w * 0.5f, cy = h * 0.5f; // mask center (geometry centered)
  const float rad = clampf(r_px, 0.f, fminf(half_x, half_y));
  std::vector<uint8_t> buf(static_cast<size_t>(w) * h * 4u, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float d =
          sd_round_rect((x + 0.5f) - cx, (y + 0.5f) - cy, half_x, half_y, rad);
      const float cov = clampf(0.5f - d, 0.f, 1.f);
      const uint8_t a = static_cast<uint8_t>(cov * 255.f + 0.5f);
      uint8_t *p = &buf[(static_cast<size_t>(y) * w + x) * 4u];
      p[0] = a; // premultiplied white: (cov,cov,cov,cov)
      p[1] = a;
      p[2] = a;
      p[3] = a;
    }
  }
  return upload_mask(r, buf, w, h);
}

// Build a ring coverage mask: a `band_px`-wide stroke hugging the INSIDE of the
// rounded rect's outer edge. coverage = inside(outer) - inside(outer shrunk by
// band), both with the same analytic ramp, so both ring edges are AA'd.
SDL_Texture *build_ring_mask(SDL_Renderer *r, int w, int h, float r_px,
                             float band_px) {
  if (w <= 0 || h <= 0 || w > kMaxMaskDim || h > kMaxMaskDim)
    return nullptr;
  const float half_x = (w - 2) * 0.5f;
  const float half_y = (h - 2) * 0.5f;
  const float cx = w * 0.5f, cy = h * 0.5f;
  const float rad = clampf(r_px, 0.f, fminf(half_x, half_y));
  const float band = clampf(band_px, 0.f, fminf(half_x, half_y));
  std::vector<uint8_t> buf(static_cast<size_t>(w) * h * 4u, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float d =
          sd_round_rect((x + 0.5f) - cx, (y + 0.5f) - cy, half_x, half_y, rad);
      const float in_outer = clampf(0.5f - d, 0.f, 1.f);
      const float in_inner = clampf(0.5f - (d + band), 0.f, 1.f);
      float cov = in_outer - in_inner;
      if (cov < 0.f)
        cov = 0.f;
      const uint8_t a = static_cast<uint8_t>(cov * 255.f + 0.5f);
      uint8_t *p = &buf[(static_cast<size_t>(y) * w + x) * 4u];
      p[0] = a;
      p[1] = a;
      p[2] = a;
      p[3] = a;
    }
  }
  return upload_mask(r, buf, w, h);
}

// Integer device extent of a point-space rect (round-out, matching the executor
// clip convention). Returns the device origin and the integer width/height.
struct DeviceBox {
  int x0, y0, w, h;
};
DeviceBox device_box(const ui::DrawRect &rect, float scale) {
  const int x0 = static_cast<int>(floorf(rect.x * scale));
  const int y0 = static_cast<int>(floorf(rect.y * scale));
  const int x1 = static_cast<int>(ceilf((rect.x + rect.w) * scale));
  const int y1 = static_cast<int>(ceilf((rect.y + rect.h) * scale));
  return {x0, y0, x1 - x0, y1 - y0};
}

// Blit `mask` (sized core+2px pad) so its central region lands on the device box
// [x0,x0+core_w]x[y0,...]; tint by premultiplied `c`; restore mod afterwards so
// a cached mask is left neutral for the next color.
void blit_mask(SDL_Renderer *r, SDL_Texture *mask, int x0, int y0, int mask_w,
               int mask_h, ui::Color c) {
  if (!mask)
    return;
  SDL_SetTextureColorMod(mask, c.r, c.g, c.b);
  SDL_SetTextureAlphaMod(mask, c.a);
  SDL_FRect dst = {static_cast<float>(x0 - 1), static_cast<float>(y0 - 1),
                   static_cast<float>(mask_w), static_cast<float>(mask_h)};
  SDL_RenderTexture(r, mask, nullptr, &dst);
  SDL_SetTextureColorMod(mask, 255, 255, 255);
  SDL_SetTextureAlphaMod(mask, 255);
}

// Premultiplied lerp of gradient stops at parameter t (stops are premultiplied
// at the IR boundary). Mirrors geometry.cpp gradient_color.
ui::Color sample_gradient(const ui::Gradient &g, float t) {
  if (g.stop_count == 0)
    return {0, 0, 0, 0};
  if (t <= g.stops[0].t)
    return g.stops[0].color;
  const int n = g.stop_count;
  if (t >= g.stops[n - 1].t)
    return g.stops[n - 1].color;
  for (int i = 0; i < n - 1; ++i) {
    const ui::GradientStop &a = g.stops[i];
    const ui::GradientStop &b = g.stops[i + 1];
    if (t >= a.t && t <= b.t) {
      const float span = b.t - a.t;
      const float f = span > 1e-6f ? (t - a.t) / span : 0.f;
      auto mix = [&](uint8_t lo, uint8_t hi) {
        return static_cast<uint8_t>(lo + (hi - lo) * f + 0.5f);
      };
      return {mix(a.color.r, b.color.r), mix(a.color.g, b.color.g),
              mix(a.color.b, b.color.b), mix(a.color.a, b.color.a)};
    }
  }
  return g.stops[n - 1].color;
}

// Quantize a device-pixel length to 1/4-px units for cache keying. 1/4-px
// granularity is a deliberate space/time tradeoff: geometries within 0.25px
// share a mask (imperceptible, and they round to the same integer raster).
inline uint64_t quant(float v) { return static_cast<uint64_t>(lroundf(v * 4.f)); }

// Pack a mask cache key from a 2-bit kind tag + mask dims + up to two quantized
// radii. Field widths (no overlap, total 56 bits):
//   w  [0..12]  h [13..25]  q0 [26..39]  q1 [40..53]  kind [54..55]
// Bounds: mask dims are capped at kMaxMaskDim (4096 -> 13 bits); a rounded
// rect's radius/band is bounded by half its size, so for masks up to 4096px the
// quantized value is <= ~8192 (14 bits). The asserts make any future
// out-of-range geometry fail loudly instead of silently colliding (the bug the
// previous hand-packed keys risked when a 12-bit field truncated large radii).
inline uint64_t make_mask_key(uint64_t kind, int w, int h, uint64_t q0,
                              uint64_t q1) {
  SDL_assert(w >= 0 && w < (1 << 13) && h >= 0 && h < (1 << 13));
  SDL_assert(q0 < (1u << 14) && q1 < (1u << 14));
  return (static_cast<uint64_t>(w) & 0x1FFF) |
         ((static_cast<uint64_t>(h) & 0x1FFF) << 13) | ((q0 & 0x3FFF) << 26) |
         ((q1 & 0x3FFF) << 40) | ((kind & 0x3) << 54);
}

} // namespace

// ---------------------------------------------------------------------------
// SdfMaskCache
// ---------------------------------------------------------------------------

SdfMaskCache::~SdfMaskCache() { clear(); }

void SdfMaskCache::clear() {
  for (Entry &e : entries_) {
    if (e.tex)
      SDL_DestroyTexture(e.tex);
    e = Entry{};
  }
  tick_ = 0;
}

SDL_Texture *SdfMaskCache::acquire(uint64_t key) {
  for (Entry &e : entries_) {
    if (e.live && e.key == key) {
      e.used = ++tick_;
      return e.tex;
    }
  }
  return nullptr;
}

SDL_Texture *SdfMaskCache::insert(uint64_t key, SDL_Texture *tex) {
  // Free slot, else LRU-evict the least-recently-used live slot.
  Entry *slot = nullptr;
  for (Entry &e : entries_) {
    if (!e.live) {
      slot = &e;
      break;
    }
    if (!slot || e.used < slot->used)
      slot = &e;
  }
  if (slot->live && slot->tex)
    SDL_DestroyTexture(slot->tex);
  slot->key = key;
  slot->tex = tex;
  slot->used = ++tick_;
  slot->live = true;
  return tex;
}

SDL_Texture *SdfMaskCache::fill_mask(SDL_Renderer *r, int w, int h,
                                     float r_px) {
  const uint64_t key = make_mask_key(1, w, h, quant(r_px), 0);
  if (SDL_Texture *hit = acquire(key))
    return hit;
  SDL_Texture *tex = build_fill_mask(r, w, h, r_px);
  return tex ? insert(key, tex) : nullptr;
}

SDL_Texture *SdfMaskCache::ring_mask(SDL_Renderer *r, int w, int h, float r_px,
                                     float band_px) {
  const uint64_t key = make_mask_key(2, w, h, quant(r_px), quant(band_px));
  if (SDL_Texture *hit = acquire(key))
    return hit;
  SDL_Texture *tex = build_ring_mask(r, w, h, r_px, band_px);
  return tex ? insert(key, tex) : nullptr;
}

// ---------------------------------------------------------------------------
// Public rasterization entry points
// ---------------------------------------------------------------------------

void sdf_fill_rounded(SDL_Renderer *r, SdfMaskCache *cache,
                      const ui::DrawRect &rect, float radius, ui::Color fill,
                      float scale) {
  if (!r || fill.a == 0)
    return;
  const DeviceBox b = device_box(rect, scale);
  if (b.w <= 0 || b.h <= 0)
    return;
  const float r_px = radius * scale;
  if (r_px <= 0.75f) {
    // Not meaningfully rounded at this resolution: a hard rect is exact.
    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_FRect q = {static_cast<float>(b.x0), static_cast<float>(b.y0),
                   static_cast<float>(b.w), static_cast<float>(b.h)};
    SDL_RenderFillRect(r, &q);
    return;
  }
  const int mw = b.w + 2, mh = b.h + 2;
  if (cache) {
    SDL_Texture *mask = cache->fill_mask(r, mw, mh, r_px);
    blit_mask(r, mask, b.x0, b.y0, mw, mh, fill);
    return;
  }
  SDL_Texture *mask = build_fill_mask(r, mw, mh, r_px);
  blit_mask(r, mask, b.x0, b.y0, mw, mh, fill);
  if (mask)
    SDL_DestroyTexture(mask);
}

namespace {
// Draw one ring (border or outline) given its outer point-space rect, outer
// radius (points), band width (points), color, at device `scale`.
void draw_ring(SDL_Renderer *r, SdfMaskCache *cache, const ui::DrawRect &outer,
               float outer_radius, float band, ui::Color color, float scale) {
  if (band <= 0.f || color.a == 0)
    return;
  const DeviceBox b = device_box(outer, scale);
  if (b.w <= 0 || b.h <= 0)
    return;
  const int mw = b.w + 2, mh = b.h + 2;
  const float r_px = outer_radius * scale;
  const float band_px = band * scale;
  if (cache) {
    SDL_Texture *mask = cache->ring_mask(r, mw, mh, r_px, band_px);
    blit_mask(r, mask, b.x0, b.y0, mw, mh, color);
    return;
  }
  SDL_Texture *mask = build_ring_mask(r, mw, mh, r_px, band_px);
  blit_mask(r, mask, b.x0, b.y0, mw, mh, color);
  if (mask)
    SDL_DestroyTexture(mask);
}
} // namespace

void sdf_frame_rounded(SDL_Renderer *r, SdfMaskCache *cache,
                       const ui::DrawRect &rect, float radius,
                       const ui::Border &border, const ui::Outline &outline,
                       float scale) {
  if (!r)
    return;

  // Border ring: hugs the inside of the border-box. Per-side widths/colors
  // collapse to one uniform ring in SDF v1 (the theme uses uniform borders);
  // width = the widest side, color = the first non-transparent side.
  const ui::SideWidths &bw = border.width;
  const float band =
      fmaxf(fmaxf(bw.top, bw.right), fmaxf(bw.bottom, bw.left));
  const ui::SideColors &bc = border.color;
  ui::Color border_color = bc.top.a ? bc.top
                           : bc.right.a ? bc.right
                           : bc.bottom.a ? bc.bottom
                                         : bc.left;
  draw_ring(r, cache, rect, radius, band, border_color, scale);

  // Outline ring: the border-box grown by outline.offset on every side
  // (offset>0 outset, <0 inset), radius grown to match, outline.width wide.
  if (outline.width > 0.f && outline.color.a) {
    const float o = outline.offset;
    const ui::DrawRect grown = {rect.x - o, rect.y - o, rect.w + 2.f * o,
                                rect.h + 2.f * o};
    const float grown_radius = fmaxf(radius + o, 0.f);
    draw_ring(r, cache, grown, grown_radius, outline.width, outline.color,
              scale);
  }
}

void sdf_gradient_rounded(SDL_Renderer *r, const ui::DrawRect &rect,
                          float radius, const ui::Gradient &gradient,
                          float scale) {
  if (!r || gradient.stop_count == 0)
    return;
  const DeviceBox b = device_box(rect, scale);
  if (b.w <= 0 || b.h <= 0 || b.w > kMaxMaskDim || b.h > kMaxMaskDim)
    return;
  const int mw = b.w + 2, mh = b.h + 2;
  const float half_x = (mw - 2) * 0.5f;
  const float half_y = (mh - 2) * 0.5f;
  const float cx = mw * 0.5f, cy = mh * 0.5f;
  const float rad = clampf(radius * scale, 0.f, fminf(half_x, half_y));

  // Gradient axis + projected extent over the rect's point-space corners.
  const float ang = gradient.angle_deg * 3.14159265358979323846f / 180.f;
  const float dx = cosf(ang), dy = sinf(ang);
  const float cxs[4] = {rect.x, rect.x + rect.w, rect.x, rect.x + rect.w};
  const float cys[4] = {rect.y, rect.y, rect.y + rect.h, rect.y + rect.h};
  float pmin = 1e30f, pmax = -1e30f;
  for (int i = 0; i < 4; ++i) {
    const float pr = cxs[i] * dx + cys[i] * dy;
    pmin = fminf(pmin, pr);
    pmax = fmaxf(pmax, pr);
  }
  const float pspan = pmax - pmin;

  // Reused scratch (UI is single-threaded, like the executor's static Scratch):
  // gradient masks are per-pixel/per-color so they can't be cached, but the
  // backing buffer can be — this avoids a heap alloc+free per gradient per frame.
  // grows monotonically to the largest gradient seen.
  static std::vector<uint8_t> buf;
  buf.assign(static_cast<size_t>(mw) * mh * 4u, 0);
  for (int y = 0; y < mh; ++y) {
    for (int x = 0; x < mw; ++x) {
      const float d =
          sd_round_rect((x + 0.5f) - cx, (y + 0.5f) - cy, half_x, half_y, rad);
      const float cov = clampf(0.5f - d, 0.f, 1.f);
      uint8_t *p = &buf[(static_cast<size_t>(y) * mw + x) * 4u];
      if (cov <= 0.f)
        continue;
      // Sample the gradient at this pixel's CENTER in point space. Mask pixel x
      // blits to device px (b.x0-1+x), whose center is (b.x0-1+x+0.5); /scale
      // converts to points. The +0.5 (pixel-center, not pixel-edge) matches both
      // the coverage SDF above (which also samples at +0.5) and the tessellated
      // modes, where GPU fragment interpolation evaluates the gradient at the
      // fragment center — so all three modes sample the ramp at the same place.
      const float ptx = (b.x0 + (x - 1) + 0.5f) / scale;
      const float pty = (b.y0 + (y - 1) + 0.5f) / scale;
      const float pr = ptx * dx + pty * dy;
      const float t = pspan > 1e-6f ? clampf((pr - pmin) / pspan, 0.f, 1.f) : 0.f;
      const ui::Color col = sample_gradient(gradient, t); // premultiplied
      p[0] = static_cast<uint8_t>(col.r * cov + 0.5f);
      p[1] = static_cast<uint8_t>(col.g * cov + 0.5f);
      p[2] = static_cast<uint8_t>(col.b * cov + 0.5f);
      p[3] = static_cast<uint8_t>(col.a * cov + 0.5f);
    }
  }
  SDL_Texture *tex = upload_mask(r, buf, mw, mh);
  if (!tex)
    return;
  SDL_FRect dst = {static_cast<float>(b.x0 - 1), static_cast<float>(b.y0 - 1),
                   static_cast<float>(mw), static_cast<float>(mh)};
  SDL_RenderTexture(r, tex, nullptr, &dst);
  SDL_DestroyTexture(tex);
}

} // namespace silencer::cppx_ui
