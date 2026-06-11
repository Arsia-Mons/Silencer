#include "texture_registry.h"

#include "sprite_bake.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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

void TextureRegistry::register_legacy_sprite(uint32_t base_id,
                                             const uint8_t *indices, int w,
                                             int h, const SDL_Color *palette256,
                                             int legacy_w, int legacy_h) {
  if (!base_id || !indices || w <= 0 || h <= 0 || !palette256)
    return;
  LegacySprite &sp = legacy_[base_id];
  sp.indices.assign(indices, indices + (size_t)w * h);
  sp.w = w;
  sp.h = h;
  sp.legacy_w = legacy_w > 0 ? legacy_w : 640;
  sp.legacy_h = legacy_h > 0 ? legacy_h : 480;
  std::memcpy(sp.palette, palette256, sizeof(sp.palette));
}

void TextureRegistry::register_legacy_nineslice(
    uint32_t base_id, const uint8_t *indices, int w, int h,
    const SDL_Color *palette256, int legacy_w, int legacy_h, int cap_l,
    int cap_r, int cap_t, int cap_b) {
  if (!base_id || !indices || w <= 0 || h <= 0 || !palette256)
    return;
  register_legacy_sprite(base_id, indices, w, h, palette256, legacy_w,
                         legacy_h);
  LegacySprite &sp = legacy_[base_id];
  sp.nine_slice = true;
  sp.cap_l = cap_l;
  sp.cap_r = cap_r;
  sp.cap_t = cap_t;
  sp.cap_b = cap_b;
}

bool TextureRegistry::resolve_legacy_nineslice_variant(
    uint32_t base_id, SDL_Renderer *renderer, float dev_x, float dev_y,
    float dev_w, float dev_h, int out_w, int out_h, LegacyVariant *out) {
  if (!renderer || !out || out_w <= 0 || out_h <= 0)
    return false;
  auto it = legacy_.find(base_id);
  if (it == legacy_.end() || !it->second.nine_slice)
    return false;
  const LegacySprite &sp = it->second;
  float s = std::min(out_w / (float)sp.legacy_w, out_h / (float)sp.legacy_h);
  if (s < 1.0f)
    s = 1.0f;
  // Software-renderer truncation, then recover the authored virtual box.
  const int x = (int)dev_x, y = (int)dev_y;
  const int vx = (int)std::floor(x / s + 0.5f);
  const int vy = (int)std::floor(y / s + 0.5f);
  const int vw = (int)std::floor(dev_w / s + 0.5f);
  const int vh = (int)std::floor(dev_h / s + 0.5f);
  if (vw < 1 || vh < 1 || vw > 4095 || vh > 4095)
    return false;
  const int tw = (int)std::ceil((vx + vw) * s) - x;
  const int th = (int)std::ceil((vy + vh) * s) - y;
  if (tw <= 0 || th <= 0 || tw > (int)(vw * s) + 9 || th > (int)(vh * s) + 9)
    return false;
  const uint64_t key = (1ull << 63) | ((uint64_t)(base_id & 0xFFFF) << 34) |
                       ((uint64_t)(((x % 18) + 18) % 18) << 29) |
                       ((uint64_t)(((y % 18) + 18) % 18) << 24) |
                       ((uint64_t)(vw & 0xFFF) << 12) | (uint64_t)(vh & 0xFFF);
  auto vit = legacy_variants_.find(key);
  uint32_t id = vit != legacy_variants_.end() ? vit->second : 0;
  if (!id) {
    std::vector<uint8_t> rgba((size_t)tw * th * 4u, 0u);
    bake_element_nineslice_rgba(sp.indices.data(), sp.w, sp.h, sp.palette, vx,
                                vy, vw, vh, sp.cap_l, sp.cap_r, sp.cap_t,
                                sp.cap_b, sp.legacy_w, sp.legacy_h, out_w,
                                out_h, x, y, tw, th, rgba.data());
    id = upload_rgba(renderer, rgba.data(), tw, th);
    if (!id)
      return false;
    legacy_variants_[key] = id;
  }
  SDL_Texture *tex = lookup(id);
  if (!tex)
    return false;
  out->texture = tex;
  out->x = x;
  out->y = y;
  out->w = tw;
  out->h = th;
  return true;
}

bool TextureRegistry::resolve_legacy_variant(uint32_t base_id,
                                             SDL_Renderer *renderer,
                                             float dev_x, float dev_y,
                                             float dev_w, float dev_h,
                                             int out_w, int out_h,
                                             LegacyVariant *out) {
  if (!renderer || !out || out_w <= 0 || out_h <= 0)
    return false;
  auto it = legacy_.find(base_id);
  if (it == legacy_.end() || it->second.nine_slice)
    return false;
  const LegacySprite &sp = it->second;
  float s = std::min(out_w / (float)sp.legacy_w, out_h / (float)sp.legacy_h);
  if (s < 1.0f)
    s = 1.0f;
  // Only 1:1-virtual draws qualify (box == the sprite cell at the menu scale).
  if (std::fabs(dev_w - sp.w * s) > 4.0f || std::fabs(dev_h - sp.h * s) > 4.0f)
    return false;
  // The software renderer truncates float dst rects; mirror it so the variant
  // lands exactly where the plain path would have drawn.
  const int x = (int)dev_x, y = (int)dev_y;
  // Nearest virtual cell. Authoring positions the box at (or 1-2 device px
  // before) the cell start, so rounding recovers the authored virtual coords.
  const int vx = (int)std::floor(x / s + 0.5f);
  const int vy = (int)std::floor(y / s + 0.5f);
  // Cover the cell's full device footprint [x, ceil((v+sprite)*s)).
  const int tw = (int)std::ceil((vx + sp.w) * s) - x;
  const int th = (int)std::ceil((vy + sp.h) * s) - y;
  if (tw <= 0 || th <= 0 || tw > (int)(sp.w * s) + 9 || th > (int)(sp.h * s) + 9)
    return false;
  const uint64_t key = ((uint64_t)base_id << 16) |
                       ((uint64_t)(((x % 18) + 18) % 18) << 8) |
                       (uint64_t)(((y % 18) + 18) % 18);
  auto vit = legacy_variants_.find(key);
  uint32_t id = vit != legacy_variants_.end() ? vit->second : 0;
  if (!id) {
    std::vector<uint8_t> rgba((size_t)tw * th * 4u, 0u);
    bake_element_rgba(sp.indices.data(), sp.w, sp.h, sp.palette, vx, vy, sp.w,
                      sp.h, sp.legacy_w, sp.legacy_h, out_w, out_h, x, y, tw,
                      th, rgba.data());
    id = upload_rgba(renderer, rgba.data(), tw, th);
    if (!id)
      return false;
    legacy_variants_[key] = id;
  }
  SDL_Texture *tex = lookup(id);
  if (!tex)
    return false;
  out->texture = tex;
  out->x = x;
  out->y = y;
  out->w = tw;
  out->h = th;
  return true;
}

void TextureRegistry::shutdown() {
  for (int i = 0; i < count_; ++i) {
    if (textures_[i])
      SDL_DestroyTexture(textures_[i]);
    textures_[i] = nullptr;
  }
  count_ = 0;
  legacy_.clear();
  legacy_variants_.clear();
}

} // namespace silencer::cppx_ui
