#pragma once

// sdf_raster — signed-distance-field rasterization of rounded vector primitives
// for RenderMode::Sdf (see render_mode.h). Each rounded shape is rendered by
// evaluating its analytic distance field per device pixel into a coverage mask
// (premultiplied white, alpha = coverage), then blitting that mask tinted by the
// shape's premultiplied color under SDL_BLENDMODE_BLEND_PREMULTIPLIED.
//
// Why this is "infinitely smooth": the coverage is computed from the exact
// distance to the true rounded-rect surface, so the curve is correct at any size
// (no tessellation facets) and the 1-device-pixel ramp is sub-pixel accurate
// (50% coverage lands on the nominal edge, matching the fringe-AA convention).
//
// The mask depends ONLY on the device-pixel geometry (size, radius, band), never
// the color — so one mask serves every color and is cached across frames. Tint
// is applied at blit via SDL color/alpha-mod: the cached mask texel (cov,cov,
// cov,cov) times the premultiplied tint (Cr,Cg,Cb,Ca) yields cov*(Cr,Cg,Cb,Ca),
// exactly the premultiplied contribution of a shape with that coverage.
//
// Lives in renderer/ (it is SDL code). Geometry types come from the SDL-free IR.

#include <SDL3/SDL.h>

#include "ui/style/visual_style.h" // ui::DrawRect, ui::Color, ui::Border, ui::Outline, ui::Gradient

#include <stdint.h>

namespace silencer::cppx_ui {

// LRU cache of SDF coverage masks keyed by device-pixel geometry. Owned by the
// renderer frame boundary (UiSurface); clear() must run before the SDL_Renderer
// is destroyed. A null cache is legal everywhere — callers then generate a
// transient mask and destroy it after the blit (correct, just uncached).
class SdfMaskCache {
public:
  ~SdfMaskCache();
  void clear();

  // Coverage mask for a filled rounded rect of `w`x`h` device px, corner radius
  // `r_px`. The shape is centered with a 1px pad so the AA ramp is never clipped.
  SDL_Texture *fill_mask(SDL_Renderer *r, int w, int h, float r_px);

  // Coverage mask for a `band_px`-wide ring hugging the inside of a rounded rect
  // of `w`x`h` device px, outer corner radius `r_px` (used for border + outline).
  SDL_Texture *ring_mask(SDL_Renderer *r, int w, int h, float r_px,
                         float band_px);

private:
  static constexpr int kCap = 96;
  struct Entry {
    uint64_t key = 0;
    SDL_Texture *tex = nullptr;
    uint64_t used = 0; // LRU stamp
    bool live = false;
  };
  Entry entries_[kCap];
  uint64_t tick_ = 0;

  // Key lookup (LRU-touch on hit) and slot insert (free slot, else LRU-evict).
  // The full device-pixel geometry is baked into `key`, so neither needs dims.
  SDL_Texture *acquire(uint64_t key);
  SDL_Texture *insert(uint64_t key, SDL_Texture *tex);
};

// Rounded solid fill via SDF. `rect`/`radius` in UI points; `scale` = device px
// per point; `fill` premultiplied. Falls back to a HARD rect when radius*scale
// <= 0.75 DEVICE px (too small to round visibly) or when the device box exceeds
// kMaxMaskDim per side (avoids an oversized mask alloc). Note: the executor only
// routes radius>0.5 POINTS here, so the device-pixel fallback only triggers at
// fractional scales — a deliberate fidelity/perf cutoff, not a missing case.
void sdf_fill_rounded(SDL_Renderer *r, SdfMaskCache *cache,
                      const ui::DrawRect &rect, float radius, ui::Color fill,
                      float scale);

// Fused frame via SDF: a uniform-width border ring (inside the border-box) plus
// the signed-offset outline ring. Per-side border colors collapse to one ring
// color in SDF mode (v1) — the theme uses uniform borders, so this is faithful.
void sdf_frame_rounded(SDL_Renderer *r, SdfMaskCache *cache,
                       const ui::DrawRect &rect, float radius,
                       const ui::Border &border, const ui::Outline &outline,
                       float scale);

// Rounded gradient fill via SDF. The mask is per-pixel (color varies), so it is
// always transient (not cached). stop_count==0 draws nothing.
void sdf_gradient_rounded(SDL_Renderer *r, const ui::DrawRect &rect,
                          float radius, const ui::Gradient &gradient,
                          float scale);

} // namespace silencer::cppx_ui
