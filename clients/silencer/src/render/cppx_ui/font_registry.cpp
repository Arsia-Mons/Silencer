#include "font_registry.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <string.h>

namespace silencer::cppx_ui {

namespace {
// Indexed by FaceId. shared/fonts/silencer-*.otf (generated from the legacy
// bitmap banks 133/134/136/132 — exact in-game identity, per SIL-6).
const char *kFaceFile[FontRegistry::FaceCount] = {
    "silencer-ui.otf",       // Body
    "silencer-ui-large.otf", // Large
    "silencer-title.otf",    // Title
    "silencer-tiny.otf",     // Tiny
};

// FNV-1a over the cache key (font_id + bytes + size + color) — a fast reject
// hash; the lookup still confirms with a full field compare.
uint64_t key_hash(uint16_t font_id, const char *text, size_t len,
                  int pixel_size, SDL_Color c) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint8_t b) { h ^= b; h *= 1099511628211ull; };
  mix(static_cast<uint8_t>(font_id));
  mix(static_cast<uint8_t>(font_id >> 8));
  for (size_t i = 0; i < len; ++i)
    mix(static_cast<uint8_t>(text[i]));
  mix(static_cast<uint8_t>(pixel_size));
  mix(static_cast<uint8_t>(pixel_size >> 8));
  mix(c.r);
  mix(c.g);
  mix(c.b);
  mix(c.a);
  return h;
}
} // namespace

FontRegistry::~FontRegistry() { shutdown(); }

bool FontRegistry::load_faces(const char *font_dir) {
  bool ok = true;
  for (int i = 0; i < FaceCount; ++i) {
    char path[1024];
    SDL_snprintf(path, sizeof(path), "%s/%s", font_dir, kFaceFile[i]);
    // Open at a nominal base; the real size is set per-query/draw via
    // TTF_SetFontSize so one face serves every size.
    faces_[i] = TTF_OpenFont(path, 16.0f);
    if (!faces_[i]) {
      SDL_Log("FontRegistry: TTF_OpenFont(%s) failed: %s", path, SDL_GetError());
      ok = false;
    }
  }
  return ok;
}

void FontRegistry::clear_text_cache() {
  for (TextEntry &e : text_cache_) {
    if (e.tex) {
      SDL_DestroyTexture(e.tex);
      e.tex = nullptr;
    }
    e.used = false;
  }
}

void FontRegistry::shutdown() {
  clear_text_cache(); // free cached textures before the renderer is destroyed
  for (int i = 0; i < FaceCount; ++i) {
    if (faces_[i]) {
      TTF_CloseFont(faces_[i]);
      faces_[i] = nullptr;
    }
  }
}

TTF_Font *FontRegistry::face(uint16_t font_id) const {
  uint16_t idx = font_id < FaceCount ? font_id : Body;
  return faces_[idx] ? faces_[idx] : faces_[Body];
}

SDL_Texture *FontRegistry::cached_text_texture(SDL_Renderer *renderer,
                                               uint16_t font_id,
                                               const char *text, size_t len,
                                               int pixel_size, SDL_Color color,
                                               int *out_w, int *out_h) {
  TTF_Font *font = face(font_id);
  if (!renderer || !font || !text || len == 0 ||
      len >= static_cast<size_t>(kTextKeyBytes) || pixel_size <= 0)
    return nullptr;

  ++text_clock_;
  const uint64_t h = key_hash(font_id, text, len, pixel_size, color);

  // Lookup: fast hash reject, then a full field compare (no collision risk).
  int lru = 0;
  uint64_t lru_tick = UINT64_MAX;
  for (int i = 0; i < kTextCacheCap; ++i) {
    TextEntry &e = text_cache_[i];
    if (!e.used) {
      if (lru_tick != 0) {
        lru = i;
        lru_tick = 0;
      } // prefer a free slot
      continue;
    }
    if (e.hash == h && e.len == static_cast<int>(len) &&
        e.font_id == font_id && e.pixel_size == pixel_size &&
        e.color.r == color.r && e.color.g == color.g &&
        e.color.b == color.b && e.color.a == color.a &&
        memcmp(e.bytes, text, len) == 0) {
      e.last_used = text_clock_;
      if (out_w)
        *out_w = e.w;
      if (out_h)
        *out_h = e.h;
      return e.tex;
    }
    if (e.last_used < lru_tick) {
      lru_tick = e.last_used;
      lru = i;
    }
  }

  // Miss: rasterize once at the requested device pixel size + straight color.
  TTF_SetFontSize(font, static_cast<float>(pixel_size));
  SDL_Surface *surface = TTF_RenderText_Blended(font, text, len, color);
  if (!surface)
    return nullptr;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
  const int tw = surface->w, th = surface->h;
  SDL_DestroySurface(surface);
  if (!tex)
    return nullptr;

  // Install into the chosen slot (free or LRU-evicted).
  TextEntry &e = text_cache_[lru];
  if (e.tex)
    SDL_DestroyTexture(e.tex);
  e.used = true;
  e.hash = h;
  e.len = static_cast<int>(len);
  e.font_id = font_id;
  e.pixel_size = pixel_size;
  e.color = color;
  memcpy(e.bytes, text, len);
  e.tex = tex;
  e.w = tw;
  e.h = th;
  e.last_used = text_clock_;
  if (out_w)
    *out_w = tw;
  if (out_h)
    *out_h = th;
  return tex;
}

} // namespace silencer::cppx_ui
