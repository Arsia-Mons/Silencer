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

// How a registered legacy sprite meets its element box in origin's virtual
// canvas (decides both the bake arithmetic and which draws qualify):
//   Cell      — drawn 1:1 at its native size (per-phase device-cell variant)
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

  // ---- Legacy virtual-grid sprites (origin menu striping parity) ----------
  // origin composites menus on a virtual int(W/s) x int(H/s) canvas and then
  // NEAREST-magnifies the whole frame by s (src = int(dx/s)) — a sprite drawn
  // 1:1 in virtual space gets its device pixel-duplication PHASE from its
  // absolute position. Registering a baked chrome texture's indexed source
  // lets the executor swap qualifying draws (the sprite at 1:1 virtual scale)
  // for a lazily-baked variant that evaluates origin's chain at the element's
  // absolute device cell. Variants memoize on (X%18, Y%18): the duplication
  // pattern period divides 18 for every quarter-integer s in play (1, 1.5,
  // 2.25, 3, 4.5). Sized fits (NineSlice/Contain/Stretch) composite into an
  // arbitrary-size virtual box first, so their variants also memoize on the
  // recovered virtual size. Caps (virtual px) apply to NineSlice only.
  void register_legacy(uint32_t base_id, const uint8_t *indices, int w, int h,
                       const SDL_Color *palette256, int legacy_w, int legacy_h,
                       LegacyFit fit, int cap_l = 0, int cap_r = 0,
                       int cap_t = 0, int cap_b = 0);

  // A resolved variant: drawn 1:1 at device (x,y), w x h texels. The rect may
  // exceed the requesting draw's box by up to 2px right/bottom — it covers the
  // sprite's full device cell (the box is Yoga-rounded, the cell is not).
  struct LegacyVariant {
    SDL_Texture *texture = nullptr;
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
    LegacyFit fit = LegacyFit::Cell;
    int cap_l = 0, cap_r = 0, cap_t = 0, cap_b = 0;
    SDL_Color palette[256] = {};
  };
  // Sized-fit resolve (NineSlice/Contain/Stretch): the box's virtual rect is
  // recovered by rounding; the variant memoizes on (base_id, X%18, Y%18, vw, vh).
  bool resolve_legacy_sized(const LegacySprite &sp, uint32_t base_id,
                            SDL_Renderer *renderer, float dev_x, float dev_y,
                            float dev_w, float dev_h, int out_w, int out_h,
                            LegacyVariant *out);
  // Cell resolve: only 1:1-virtual draws qualify; memoizes on (base_id, X%18, Y%18).
  bool resolve_legacy_cell(const LegacySprite &sp, uint32_t base_id,
                           SDL_Renderer *renderer, float dev_x, float dev_y,
                           float dev_w, float dev_h, int out_w, int out_h,
                           LegacyVariant *out);

  std::map<uint32_t, LegacySprite> legacy_;     // base_id -> indexed source
  std::map<uint64_t, uint32_t> legacy_variants_; // (base_id, X%18, Y%18[, vw, vh]) -> id
};

} // namespace silencer::cppx_ui
