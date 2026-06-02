#pragma once

// SIL-11 cppx UI renderer bridge. Multi-face TTF registry for the retained UI
// (SIL-6 decision 1): font_id maps to the four silencer faces. The measure seam
// (ui::set_text_measurer) and the draw executor's text path both read faces
// from here, so measure == paint. This is renderer-side code — it owns SDL_ttf;
// the SDL-free ui/ runtime never touches it.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdint.h>

namespace silencer::cppx_ui {

class FontRegistry {
public:
  // font_id contract: 0=body (silencer-ui), 1=large (ui-large), 2=title,
  // 3=tiny (HUD digits), 4=heading (bank-135, the dominant legacy title/heading
  // face — SIL-95). Matches doc §SIL-6 decision 1's face wiring.
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

  // Open all four faces from <font_dir>/silencer-{ui,ui-large,title,tiny}.otf.
  // TTF_Init() must have been called. Returns false if any face fails to open
  // (the others still load; face() falls back to Body for missing faces).
  bool load_faces(const char *font_dir);
  void shutdown();

  // The TTF face for a font_id. Out-of-range ids and unopened faces fall back to
  // Body. Null only if nothing loaded. Size is applied per-query via
  // TTF_SetFontSize at measure/paint time, so faces open at a nominal base size.
  TTF_Font *face(uint16_t font_id) const;
  TTF_Font *default_font() const { return face(Body); }
  bool loaded() const { return faces_[Body] != nullptr; }

  // Rasterize `text` (len bytes) with face(font_id) at `pixel_size` in
  // STRAIGHT-alpha `color`, returning a REGISTRY-OWNED SDL_Texture (do NOT
  // destroy) sized *out_w x *out_h. Cached + keyed by (font_id, bytes,
  // pixel_size, color) so a string repeated across frames rasterizes ONCE.
  // Returns nullptr for empty / over-long (>= 64 bytes) text or on failure —
  // the caller renders those uncached. (Renderer-coupled paint path; the
  // measure path needs no renderer.) Adapted from golden FontRegistry; the
  // Silencer change is the per-face key + face(font_id) raster.
  SDL_Texture *cached_text_texture(SDL_Renderer *renderer, uint16_t font_id,
                                   const char *text, size_t len, int pixel_size,
                                   SDL_Color color, int *out_w, int *out_h);

private:
  void clear_text_cache();

  TTF_Font *faces_[FaceCount] = {};

  // Fixed-capacity LRU cache of rasterized text textures (golden FontRegistry).
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
