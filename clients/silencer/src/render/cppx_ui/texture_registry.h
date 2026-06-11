#pragma once

// SIL-11: the renderer-side texture_id -> SDL_Texture* map. The ONLY place a
// ui/ texture_id (the opaque uint32_t carried in the draw IR's ImageData arm)
// meets a real SDL handle (doc §3: "texture_id never appears in any public
// prop"). ui/ never sees SDL_Texture; the executor asks this registry by id.
// Textures are stored premultiplied so the executor draws them under
// SDL_BLENDMODE_BLEND_PREMULTIPLIED. Adapted from golden renderer/texture_registry
// (namespace + the Silencer cap note); logic verbatim.

#include <SDL3/SDL.h>

#include <stdint.h>

#include <map>
#include <vector>

namespace silencer::cppx_ui {

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

  // ---- Legacy virtual-grid sprites (origin menu striping parity) ----------
  // origin composites menus on a virtual int(W/s) x int(H/s) canvas and then
  // NEAREST-magnifies the whole frame by s (src = int(dx/s)) — a sprite drawn
  // 1:1 in virtual space gets its device pixel-duplication PHASE from its
  // absolute position. Registering a baked chrome texture's indexed source
  // lets the executor swap qualifying draws (the sprite at 1:1 virtual scale)
  // for a lazily-baked variant that evaluates origin's chain at the element's
  // absolute device cell. Variants memoize on (X%18, Y%18): the duplication
  // pattern period divides 18 for every quarter-integer s in play (1, 1.5,
  // 2.25, 3, 4.5).
  void register_legacy_sprite(uint32_t base_id, const uint8_t *indices, int w,
                              int h, const SDL_Color *palette256, int legacy_w,
                              int legacy_h);

  // Nine-slice flavor (the metal-chrome buttons, origin
  // DispatchButtonNineSlice): the sprite composites into an arbitrary-size
  // virtual box (cropped corners, TILED edges/center) before the whole-frame
  // magnify, so variants depend on the element's virtual size as well as its
  // phase. Caps are sprite/virtual px.
  void register_legacy_nineslice(uint32_t base_id, const uint8_t *indices,
                                 int w, int h, const SDL_Color *palette256,
                                 int legacy_w, int legacy_h, int cap_l,
                                 int cap_r, int cap_t, int cap_b);

  // Contain flavor (origin DispatchImage Contain — agency emblems): the
  // sprite letterboxes into an arbitrary-size virtual box before the
  // whole-frame magnify.
  void register_legacy_contain(uint32_t base_id, const uint8_t *indices,
                               int w, int h, const SDL_Color *palette256,
                               int legacy_w, int legacy_h);

  // Stretch flavor (origin DispatchImage Stretch / nine-slice-less plates
  // sized to their box): the sprite stretches into an arbitrary-size virtual
  // box before the whole-frame magnify.
  void register_legacy_stretch(uint32_t base_id, const uint8_t *indices,
                               int w, int h, const SDL_Color *palette256,
                               int legacy_w, int legacy_h);

  // A resolved variant: drawn 1:1 at device (x,y), w x h texels. The rect may
  // exceed the requesting draw's box by up to 2px right/bottom — it covers the
  // sprite's full device cell (the box is Yoga-rounded, the cell is not).
  struct LegacyVariant {
    SDL_Texture *texture = nullptr;
    int x = 0, y = 0, w = 0, h = 0;
  };

  // Resolve (bake on first use) the per-phase variant for a draw with device
  // rect (dev_x,dev_y,dev_w,dev_h) on an out_w x out_h output. False when the
  // id isn't registered or the draw isn't the sprite at 1:1 virtual scale —
  // caller falls through to the plain path.
  bool resolve_legacy_variant(uint32_t base_id, SDL_Renderer *renderer,
                              float dev_x, float dev_y, float dev_w,
                              float dev_h, int out_w, int out_h,
                              LegacyVariant *out);

  // Nine-slice resolve: any box size qualifies (the slice stretches); the
  // box's virtual rect is recovered by rounding and the variant memoizes on
  // (base_id, X%18, Y%18, vw, vh).
  bool resolve_legacy_nineslice_variant(uint32_t base_id,
                                        SDL_Renderer *renderer, float dev_x,
                                        float dev_y, float dev_w, float dev_h,
                                        int out_w, int out_h,
                                        LegacyVariant *out);

  // Contain resolve: like the nine-slice flavor (any box size; memo includes
  // the recovered virtual size).
  bool resolve_legacy_contain_variant(uint32_t base_id, SDL_Renderer *renderer,
                                      float dev_x, float dev_y, float dev_w,
                                      float dev_h, int out_w, int out_h,
                                      LegacyVariant *out);

  void shutdown();

private:
  // Raised from 64: the per-phase legacy variants (sprites, nine-slice
  // buttons, contain emblems, stretch plates) accumulate across a session —
  // a 13-screen capture run measurably exceeds 64 and silently fell back to
  // raw draws (game_staging regression, 2026-06-11). Atlasing remains the
  // long-term relief valve.
  static constexpr int kMaxTextures = 256;
  SDL_Texture *textures_[kMaxTextures] = {};
  int count_ = 0;

  struct LegacySprite {
    std::vector<uint8_t> indices;
    int w = 0, h = 0;
    int legacy_w = 640, legacy_h = 480;
    bool nine_slice = false;
    bool contain = false;
    bool stretch = false;
    int cap_l = 0, cap_r = 0, cap_t = 0, cap_b = 0;
    SDL_Color palette[256] = {};
  };
  std::map<uint32_t, LegacySprite> legacy_;     // base_id -> indexed source
  std::map<uint64_t, uint32_t> legacy_variants_; // (base_id, X%18, Y%18[, vw, vh]) -> id
};

} // namespace silencer::cppx_ui
