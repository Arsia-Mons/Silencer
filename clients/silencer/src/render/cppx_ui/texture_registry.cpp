#include "texture_registry.h"

#include <vector>

namespace silencer::cppx_ui {

TextureRegistry::~TextureRegistry() { shutdown(); }

uint32_t TextureRegistry::adopt(SDL_Texture *texture) {
  if (!texture || count_ >= kMaxTextures)
    return 0;
  // Premultiplied compositing for every UI texture.
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
  textures_[count_++] = texture;
  return static_cast<uint32_t>(count_); // id == 1-based slot (0 reserved "none")
}

uint32_t TextureRegistry::upload_rgba(SDL_Renderer *renderer,
                                      const uint8_t *rgba, int width,
                                      int height) {
  if (!renderer || !rgba || width <= 0 || height <= 0)
    return 0;

  // Premultiply at upload: the executor draws every texture under
  // SDL_BLENDMODE_BLEND_PREMULTIPLIED, so stored texels must be premultiplied.
  std::vector<uint8_t> pm(static_cast<size_t>(width) * height * 4u);
  for (size_t i = 0; i < pm.size(); i += 4) {
    const int a = rgba[i + 3];
    pm[i + 0] = static_cast<uint8_t>(rgba[i + 0] * a / 255);
    pm[i + 1] = static_cast<uint8_t>(rgba[i + 1] * a / 255);
    pm[i + 2] = static_cast<uint8_t>(rgba[i + 2] * a / 255);
    pm[i + 3] = static_cast<uint8_t>(a);
  }

  SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, width, height);
  if (!tex)
    return 0;
  if (!SDL_UpdateTexture(tex, nullptr, pm.data(),
                         width * 4)) { // pitch = width*4 (tightly packed)
    SDL_DestroyTexture(tex);
    return 0;
  }
  // Nearest sampling keeps goldens crisp and deterministic; nine-slice corners
  // are 1:1 anyway. Callers wanting smoothing can override post-adopt.
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
  return adopt(tex);
}

SDL_Texture *TextureRegistry::lookup(uint32_t id) const {
  if (id == 0 || id > static_cast<uint32_t>(count_))
    return nullptr;
  return textures_[id - 1];
}

void TextureRegistry::shutdown() {
  for (int i = 0; i < count_; ++i) {
    if (textures_[i])
      SDL_DestroyTexture(textures_[i]);
    textures_[i] = nullptr;
  }
  count_ = 0;
}

} // namespace silencer::cppx_ui
