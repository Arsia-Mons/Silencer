#pragma once

// The renderer-side texture_id -> SDL_Texture* map: the only place a ui/
// texture_id meets a real SDL handle. Textures are stored premultiplied
// (drawn under SDL_BLENDMODE_BLEND_PREMULTIPLIED).

#include <SDL3/SDL.h>

#include <stdint.h>

#include <map>
#include <vector>

namespace silencer::cppx_ui {

// How a registered legacy sprite meets its element box (decides the bake
// arithmetic and which draws qualify):
//   Cell      — drawn 1:1 at its native size
//   NineSlice — cropped corners + tiled bands
//   Contain   — letterboxed into the box
//   Stretch   — stretched to the box
enum class LegacyFit : uint8_t { Cell, NineSlice, Contain, Stretch };

class TextureRegistry {
public:
  TextureRegistry() = default;
  ~TextureRegistry();

  TextureRegistry(const TextureRegistry &) = delete;
  TextureRegistry &operator=(const TextureRegistry &) = delete;

  // Adopts ownership of `texture` and returns a nonzero id (0 if null or full).
  // The texture is expected premultiplied (set here too).
  uint32_t adopt(SDL_Texture *texture);

  // Uploads tightly-packed straight-alpha RGBA8888, premultiplying at upload;
  // returns its id (0 on failure).
  uint32_t upload_rgba(SDL_Renderer *renderer, const uint8_t *rgba, int width,
                       int height);

  // Maps an id back to its SDL_Texture* (nullptr for id==0 or unknown).
  SDL_Texture *lookup(uint32_t id) const;

  // The premultiplied RGBA bytes (+ dims) backing slot `id` for the GPU emitter
  // to upload. False for id==0 / unknown / a slot adopted without source bytes
  // (only upload_rgba retains them; adopt does not). Valid until shutdown().
  bool gpu_texture(uint32_t id, const uint8_t **rgba, int *w, int *h) const;

  // ---- Legacy virtual-grid sprites (canonical-phase bake) ----
  // origin NEAREST-magnified the whole virtual frame (src = int(dx/s)), so a
  // sprite's device pixels and size depended on its absolute position.
  // Registering a baked chrome texture's indexed source lets the executor swap
  // qualifying draws for a lazily-baked canonical variant evaluated at phase 0,
  // pixel-identical and identically sized everywhere. Variants memoize on
  // (base_id, s) — sized fits also on the recovered virtual size. Caps (virtual
  // px) apply to NineSlice only.
  void register_legacy(uint32_t base_id, const uint8_t *indices, int w, int h,
                       const SDL_Color *palette256, int legacy_w, int legacy_h,
                       LegacyFit fit, int cap_l = 0, int cap_r = 0,
                       int cap_t = 0, int cap_b = 0);

  // A resolved variant: drawn 1:1 at device (x,y), w x h texels. The rect may
  // exceed the draw's box by up to 2px right/bottom (it covers the full device
  // cell; the box is Yoga-rounded, the cell is not).
  struct LegacyVariant {
    SDL_Texture *texture = nullptr;
    uint32_t id = 0; // registry slot
    int x = 0, y = 0, w = 0, h = 0;
  };

  // Resolve (bake on first use) the variant for a draw, dispatching on the
  // registered fit. False when the id isn't registered or (Cell fit) the draw
  // isn't the sprite at 1:1 virtual scale — caller falls through to the plain
  // path.
  bool resolve_legacy(uint32_t base_id, SDL_Renderer *renderer, float dev_x,
                      float dev_y, float dev_w, float dev_h, int out_w,
                      int out_h, LegacyVariant *out);

  void shutdown();

private:
  static constexpr int kMaxTextures = 256;
  SDL_Texture *textures_[kMaxTextures] = {};
  // Resident premultiplied source bytes per slot (GPU emitter uploads once);
  // empty for a slot adopted without bytes.
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
  // Sized-fit resolve (NineSlice/Contain/Stretch): memoizes on (base_id, s, vw, vh).
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
