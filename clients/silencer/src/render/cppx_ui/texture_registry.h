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

  void shutdown();

private:
  // 64-texture cap (doc §3: do not raise speculatively; atlasing only if >64
  // chrome textures is proven, coordinated upstream).
  static constexpr int kMaxTextures = 64;
  SDL_Texture *textures_[kMaxTextures] = {};
  int count_ = 0;
};

} // namespace silencer::cppx_ui
