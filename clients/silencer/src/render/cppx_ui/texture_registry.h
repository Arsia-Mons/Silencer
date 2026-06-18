#pragma once

// The renderer-side texture_id -> SDL_Texture* map. The ONLY place a
// ui/ texture_id (the opaque uint32_t carried in the draw IR's ImageData arm)
// meets a real SDL handle (doc §3: "texture_id never appears in any public
// prop"). ui/ never sees SDL_Texture; the executor asks this registry by id.
// Textures are stored premultiplied so the executor draws them under
// SDL_BLENDMODE_BLEND_PREMULTIPLIED.

#include <SDL3/SDL.h>

#include <stdint.h>

#include <map>
#include <vector>

namespace silencer::cppx_ui {

// How a registered legacy sprite meets its element box in origin's virtual
// canvas (decides both the bake arithmetic and which draws qualify):
//   Cell      — drawn 1:1 at its native size
//   NineSlice — composited into the box with cropped corners + tiled bands
//   Contain   — letterboxed into the box (origin DispatchImage Contain)
//   Stretch   — stretched to the box (origin DispatchImage Stretch)
enum class LegacyFit : uint8_t { Cell, NineSlice, Contain, Stretch };

class TextureRegistry {
public:
  TextureRegistry() = default;
  ~TextureRegistry();

  TextureRegistry(const TextureRegistry &) = delete;
  TextureRegistry &operator=(const TextureRegistry &) = delete;

  // Adopts ownership of `texture` and returns a nonzero id for ImageData::
  // texture_id. The texture is expected premultiplied (set here too). Returns 0
  // if null or the registry is full.
  uint32_t adopt(SDL_Texture *texture);

  // Uploads tightly-packed RGBA8888 STRAIGHT-alpha pixels as a premultiplied
  // texture (premultiplied at upload), returning its id (0 on failure). The
  // sprite->RGBA bake's opaque/transparent output is straight==premultiplied,
  // so it feeds this directly. `renderer` is the live UI renderer.
  uint32_t upload_rgba(SDL_Renderer *renderer, const uint8_t *rgba, int width,
                       int height);

  // Maps an id back to its SDL_Texture* (nullptr for id==0 or unknown).
  SDL_Texture *lookup(uint32_t id) const;

  // The premultiplied RGBA bytes (+ dims) backing slot `id`, for the
  // GPU emitter to upload as a resident SDL_GPUTexture. Returns false for
  // id==0 / unknown / a slot adopted without source bytes. The bytes are owned
  // here and valid until the next shutdown(). `upload_rgba` (and thus every
  // chrome/backdrop/legacy-variant bake) retains them; `adopt` does not.
  bool gpu_texture(uint32_t id, const uint8_t **rgba, int *w, int *h) const;

  // ---- Legacy virtual-grid sprites (canonical-phase bake) ----
  // origin composited menus on a virtual int(W/s) x int(H/s) canvas and then
  // NEAREST-magnified the whole frame by s (src = int(dx/s)) — a sprite's
  // device pixels (and size) depended on its absolute position, an accident
  // of the pipeline. Registering a baked chrome texture's indexed source lets
  // the executor swap qualifying draws (the sprite at 1:1 virtual scale, or a
  // sized fit) for a lazily-baked CANONICAL variant: origin's chain evaluated
  // at phase 0 (src = int(lx/s), lx from 0), so every instance of a sprite is
  // pixel-identical and identically sized. Variants memoize on (base_id, s) —
  // sized fits (NineSlice/Contain/Stretch) composite into an arbitrary-size
  // virtual box first, so theirs also memoize on the recovered virtual size.
  // Caps (virtual px) apply to NineSlice only.
  void register_legacy(uint32_t base_id, const uint8_t *indices, int w, int h,
                       const SDL_Color *palette256, int legacy_w, int legacy_h,
                       LegacyFit fit, int cap_l = 0, int cap_r = 0,
                       int cap_t = 0, int cap_b = 0);

  // A resolved variant: drawn 1:1 at device (x,y), w x h texels. The rect may
  // exceed the requesting draw's box by up to 2px right/bottom — it covers the
  // sprite's full device cell (the box is Yoga-rounded, the cell is not).
  struct LegacyVariant {
    SDL_Texture *texture = nullptr;
    uint32_t id = 0; // registry slot (GPU emitter: ui_texture_key::image)
    int x = 0, y = 0, w = 0, h = 0;
  };

  // Resolve (bake on first use) the variant for a draw with device rect
  // (dev_x,dev_y,dev_w,dev_h) on an out_w x out_h output, dispatching on the
  // registered fit. False when the id isn't registered or (Cell fit) the draw
  // isn't the sprite at 1:1 virtual scale — caller falls through to the plain
  // path.
  bool resolve_legacy(uint32_t base_id, SDL_Renderer *renderer, float dev_x,
                      float dev_y, float dev_w, float dev_h, int out_w,
                      int out_h, LegacyVariant *out);

  void shutdown();

private:
  // Raised from 64 when variants were per-phase (X%18, Y%18) and a 13-screen
  // capture run exceeded it (game_staging regression, 2026-06-11). Canonical
  // variants (U-2) collapse to one per sprite/size, but the headroom stays —
  // ramp/team palette variants register many base sprites.
  static constexpr int kMaxTextures = 256;
  SDL_Texture *textures_[kMaxTextures] = {};
  // The premultiplied source bytes kept resident per slot so the GPU
  // emitter can upload them once. Empty for a slot adopted without bytes.
  std::vector<uint8_t> rgba_[kMaxTextures];
  int tw_[kMaxTextures] = {};
  int th_[kMaxTextures] = {};
  int count_ = 0;

  struct LegacySprite {
    std::vector<uint8_t> indices;
    int w = 0, h = 0;
    int legacy_w = 640, legacy_h = 480;
    LegacyFit fit = LegacyFit::Cell;
    int cap_l = 0, cap_r = 0, cap_t = 0, cap_b = 0;
    SDL_Color palette[256] = {};
  };
  // Sized-fit resolve (NineSlice/Contain/Stretch): the box's virtual SIZE is
  // recovered by rounding; the variant memoizes on (base_id, s, vw, vh).
  bool resolve_legacy_sized(const LegacySprite &sp, uint32_t base_id,
                            SDL_Renderer *renderer, float dev_x, float dev_y,
                            float dev_w, float dev_h, int out_w, int out_h,
                            LegacyVariant *out);
  // Cell resolve: only 1:1-virtual draws qualify; memoizes on (base_id, s).
  bool resolve_legacy_cell(const LegacySprite &sp, uint32_t base_id,
                           SDL_Renderer *renderer, float dev_x, float dev_y,
                           float dev_w, float dev_h, int out_w, int out_h,
                           LegacyVariant *out);

  std::map<uint32_t, LegacySprite> legacy_;     // base_id -> indexed source
  std::map<uint64_t, uint32_t> legacy_variants_; // (base_id, s[, vw, vh]) -> id
};

} // namespace silencer::cppx_ui
