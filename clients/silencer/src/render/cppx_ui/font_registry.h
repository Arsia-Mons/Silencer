#pragma once

// Multi-face TTF registry for the retained UI: font_id -> face. The measure
// seam and the draw executor's text path both read faces from here, so
// measure == paint. The only owner of SDL_ttf; the ui/ runtime never sees it.

#include <SDL3/SDL.h>
typedef struct TTF_Font TTF_Font;

#include <stdint.h>

namespace silencer::cppx_ui {

class FontRegistry {
public:
  enum FaceId : uint16_t {
    Body = 0,
    Large = 1,
    Title = 2,
    Tiny = 3,
    Heading = 4,
    FaceCount = 5
  };

  FontRegistry() = default;
  ~FontRegistry();
  FontRegistry(const FontRegistry &) = delete;
  FontRegistry &operator=(const FontRegistry &) = delete;

  // Open all faces from font_dir. TTF_Init() must have been called. Returns
  // false if any face fails to open (the rest still load; face() falls back to
  // Body for missing faces).
  bool load_faces(const char *font_dir);
  void shutdown();

  // The TTF face for a font_id (Body fallback for out-of-range/unopened ids;
  // null only if nothing loaded). Size is applied per-query via TTF_SetFontSize.
  TTF_Font *face(uint16_t font_id) const;
  TTF_Font *default_font() const { return face(Body); }
  bool loaded() const { return faces_[Body] != nullptr; }

  // Rasterize `text` (len bytes) with face(font_id) at `pixel_size` in
  // straight-alpha `color`, returning a registry-owned SDL_Texture (do NOT
  // destroy) sized *out_w x *out_h. Cached + keyed by (font_id, bytes,
  // pixel_size, color) so a repeated string rasterizes once. Returns nullptr for
  // empty / over-long (>= 64 bytes) text or on failure — caller renders uncached.
  SDL_Texture *cached_text_texture(SDL_Renderer *renderer, uint16_t font_id,
                                   const char *text, size_t len, int pixel_size,
                                   SDL_Color color, int *out_w, int *out_h);

private:
  void clear_text_cache();

  TTF_Font *faces_[FaceCount] = {};

  // Fixed-capacity LRU cache of rasterized text textures.
  static constexpr int kTextCacheCap = 128;
  static constexpr int kTextKeyBytes = 64;
  struct TextEntry {
    bool used = false;
    uint64_t hash = 0;
    int len = 0;
    uint16_t font_id = 0;
    int pixel_size = 0;
    SDL_Color color = {0, 0, 0, 0};
    char bytes[kTextKeyBytes] = {};
    SDL_Texture *tex = nullptr;
    int w = 0;
    int h = 0;
    uint64_t last_used = 0;
  };
  TextEntry text_cache_[kTextCacheCap] = {};
  uint64_t text_clock_ = 0;
};

} // namespace silencer::cppx_ui
