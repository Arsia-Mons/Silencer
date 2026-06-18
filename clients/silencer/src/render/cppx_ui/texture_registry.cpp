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
  const uint32_t id = adopt(tex);
  if (id) {
    // SIL-240: keep the premultiplied bytes resident so the GPU emitter can
    // upload them once (chrome / backdrop / glyph / legacy-variant bakes all
    // flow through here).
    rgba_[id - 1] = std::move(pm);
    tw_[id - 1] = width;
    th_[id - 1] = height;
  }
  return id;
}

SDL_Texture *TextureRegistry::lookup(uint32_t id) const {
  if (id == 0 || id > static_cast<uint32_t>(count_))
    return nullptr;
  return textures_[id - 1];
}

bool TextureRegistry::gpu_texture(uint32_t id, const uint8_t **rgba, int *w,
                                  int *h) const {
  if (id == 0 || id > static_cast<uint32_t>(count_))
    return false;
  const std::vector<uint8_t> &bytes = rgba_[id - 1];
  if (bytes.empty())
    return false;
  *rgba = bytes.data();
  *w = tw_[id - 1];
  *h = th_[id - 1];
  return true;
}

void TextureRegistry::register_legacy(uint32_t base_id,
                                      const uint8_t *indices, int w, int h,
                                      const SDL_Color *palette256,
                                      int legacy_w, int legacy_h,
                                      LegacyFit fit, int cap_l, int cap_r,
                                      int cap_t, int cap_b) {
  if (!base_id || !indices || w <= 0 || h <= 0 || !palette256)
    return;
  LegacySprite &sp = legacy_[base_id];
  sp.indices.assign(indices, indices + (size_t)w * h);
  sp.w = w;
  sp.h = h;
  sp.legacy_w = legacy_w > 0 ? legacy_w : 640;
  sp.legacy_h = legacy_h > 0 ? legacy_h : 480;
  sp.fit = fit;
  sp.cap_l = cap_l;
  sp.cap_r = cap_r;
  sp.cap_t = cap_t;
  sp.cap_b = cap_b;
  std::memcpy(sp.palette, palette256, sizeof(sp.palette));
}

bool TextureRegistry::resolve_legacy(uint32_t base_id, SDL_Renderer *renderer,
                                     float dev_x, float dev_y, float dev_w,
                                     float dev_h, int out_w, int out_h,
                                     LegacyVariant *out) {
  if (!renderer || !out || out_w <= 0 || out_h <= 0)
    return false;
  auto it = legacy_.find(base_id);
  if (it == legacy_.end())
    return false;
  const LegacySprite &sp = it->second;
  return sp.fit == LegacyFit::Cell
             ? resolve_legacy_cell(sp, base_id, renderer, dev_x, dev_y, dev_w,
                                   dev_h, out_w, out_h, out)
             : resolve_legacy_sized(sp, base_id, renderer, dev_x, dev_y,
                                    dev_w, dev_h, out_w, out_h, out);
}

bool TextureRegistry::resolve_legacy_sized(
    const LegacySprite &sp, uint32_t base_id, SDL_Renderer *renderer,
    float dev_x, float dev_y, float dev_w, float dev_h, int out_w, int out_h,
    LegacyVariant *out) {
  float s = std::min(out_w / (float)sp.legacy_w, out_h / (float)sp.legacy_h);
  if (s < 1.0f)
    s = 1.0f;
  // Recover the authored virtual box SIZE (design metric); position no longer
  // participates (canonical phase, U-2/SIL-204).
  const int vw = (int)std::floor(dev_w / s + 0.5f);
  const int vh = (int)std::floor(dev_h / s + 0.5f);
  if (vw < 1 || vh < 1 || vw > 4095 || vh > 4095)
    return false;
  // Floor-quantized device cell (legacy SW dst-rect floor — s==1 in-game
  // draws land byte-identical); footprint = the canonical chain's ceil(v*s).
  const int x = (int)std::floor(dev_x), y = (int)std::floor(dev_y);
  const int tw = (int)std::ceil(vw * s - 0.001f);
  const int th = (int)std::ceil(vh * s - 0.001f);
  if (tw <= 0 || th <= 0)
    return false;
  // One variant per (sprite, scale, virtual box size). Scale is in the key:
  // after a window resize the same box recurs at a different magnify and a
  // stale-scale texture would draw at its bake-time dims.
  const int s_q = (int)(s * 1000.0f + 0.5f);
  const uint64_t key = (1ull << 63) | ((uint64_t)(base_id & 0xFFFF) << 40) |
                       ((uint64_t)(s_q & 0xFFFF) << 24) |
                       ((uint64_t)(vw & 0xFFF) << 12) | (uint64_t)(vh & 0xFFF);
  auto vit = legacy_variants_.find(key);
  uint32_t id = vit != legacy_variants_.end() ? vit->second : 0;
  if (!id) {
    std::vector<uint8_t> rgba((size_t)tw * th * 4u, 0u);
    if (sp.fit == LegacyFit::NineSlice)
      bake_canonical_nineslice_rgba(sp.indices.data(), sp.w, sp.h, sp.palette,
                                    vw, vh, sp.cap_l, sp.cap_r, sp.cap_t,
                                    sp.cap_b, s, tw, th, rgba.data());
    else if (sp.fit == LegacyFit::Contain)
      bake_canonical_contain_rgba(sp.indices.data(), sp.w, sp.h, sp.palette,
                                  vw, vh, s, tw, th, rgba.data());
    else // LegacyFit::Stretch (origin DispatchImage Stretch)
      bake_canonical_stretch_rgba(sp.indices.data(), sp.w, sp.h, sp.palette,
                                  vw, vh, s, tw, th, rgba.data());
    id = upload_rgba(renderer, rgba.data(), tw, th);
    if (!id)
      return false;
    legacy_variants_[key] = id;
  }
  SDL_Texture *tex = lookup(id);
  if (!tex)
    return false;
  out->texture = tex;
  out->id = id;
  out->x = x;
  out->y = y;
  out->w = tw;
  out->h = th;
  return true;
}

bool TextureRegistry::resolve_legacy_cell(
    const LegacySprite &sp, uint32_t base_id, SDL_Renderer *renderer,
    float dev_x, float dev_y, float dev_w, float dev_h, int out_w, int out_h,
    LegacyVariant *out) {
  float s = std::min(out_w / (float)sp.legacy_w, out_h / (float)sp.legacy_h);
  if (s < 1.0f)
    s = 1.0f;
  // Only 1:1-virtual draws qualify (box == the sprite cell at the menu scale).
  if (std::fabs(dev_w - sp.w * s) > 4.0f || std::fabs(dev_h - sp.h * s) > 4.0f)
    return false;
  // Canonical cell (U-2/SIL-204): ONE variant per (sprite, scale), drawn at
  // the floor-quantized device position (legacy SW dst-rect floor — s==1
  // in-game draws land byte-identical). Same sprite, same pixels, same size,
  // anywhere on screen.
  const int x = (int)std::floor(dev_x), y = (int)std::floor(dev_y);
  const int tw = (int)std::ceil(sp.w * s - 0.001f);
  const int th = (int)std::ceil(sp.h * s - 0.001f);
  if (tw <= 0 || th <= 0)
    return false;
  const int s_q = (int)(s * 1000.0f + 0.5f);
  const uint64_t key = ((uint64_t)base_id << 16) | (uint64_t)(s_q & 0xFFFF);
  auto vit = legacy_variants_.find(key);
  uint32_t id = vit != legacy_variants_.end() ? vit->second : 0;
  if (!id) {
    std::vector<uint8_t> rgba((size_t)tw * th * 4u, 0u);
    bake_canonical_cell_rgba(sp.indices.data(), sp.w, sp.h, sp.palette, s, tw,
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
  out->id = id;
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
    rgba_[i].clear();
    rgba_[i].shrink_to_fit();
    tw_[i] = 0;
    th_[i] = 0;
  }
  count_ = 0;
  legacy_.clear();
  legacy_variants_.clear();
}

} // namespace silencer::cppx_ui
