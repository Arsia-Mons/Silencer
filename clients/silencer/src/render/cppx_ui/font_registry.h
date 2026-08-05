#pragma once

// Multi-face TTF registry for the retained UI: font_id -> face. The measure
// seam and the draw executor's text path both read faces from here, so
// measure == paint. The only owner of SDL_ttf; the ui/ runtime never sees it.
//
// Text renders from a per-glyph cache, not per-string textures: each
// (face, pixel_size) gets its own fixed-size TTF_Font (never resized —
// TTF_SetFontSize flushes SDL_ttf's internal glyph cache, so one shared
// resized face defeats all caching underneath). draw_text lets SDL_ttf's own
// layout engine position the run (fractional pen, kerning — the same layout
// TTF_GetStringSize measures), then blits the positioned glyphs from textures
// each glyph rasterizes into once, ever. Steady-state repaint cost is
// therefore bounded by the visible glyph quads regardless of how many
// distinct strings the UI holds.

#include <SDL3/SDL.h>
typedef struct TTF_Font TTF_Font;

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>

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
  // false if any face fails to open (the rest still load; sizing falls back to
  // Body for missing faces).
  bool load_faces(const char *font_dir);
  void shutdown();
  bool loaded() const { return faces_[Body] != nullptr; }

  // The face for font_id at pixel_size (Body fallback for out-of-range or
  // unopened ids; null only if nothing loaded). Created on demand, hinting
  // disabled (generated pixel fonts; see load_faces), and permanently at that
  // size — callers never TTF_SetFontSize.
  TTF_Font *sized_face(uint16_t font_id, int pixel_size);

  // Draw `text` (len bytes, UTF-8) with the sized face at (x, y) — the top
  // left of the line box, device px — tinted by straight-alpha `color`.
  bool draw_text(SDL_Renderer *renderer, uint16_t font_id, int pixel_size,
                 const char *text, size_t len, SDL_Color color, float x,
                 float y);

  // Destroy the glyph textures (they are bound to the current renderer) but
  // keep the fonts. Call when the renderer is recreated; glyphs re-rasterize
  // lazily against the new one.
  void drop_textures();

  // Total glyph rasterizations performed. Steady-state rendering of unchanged
  // content must not grow this — repaint cost is otherwise unbounded (each
  // rasterization is an outline raster + a texture create).
  uint64_t rasterizations() const { return rasterizations_; }

private:
  // One rasterized glyph: white coverage, tinted per draw via color/alpha
  // mod. tex == null with loaded == true is a blank/failed glyph.
  struct Glyph {
    SDL_Texture *tex = nullptr;
    bool loaded = false;
  };

  struct SizedFont {
    TTF_Font *font = nullptr;
    uint16_t face_id = 0;
    int pixel_size = 0;
    std::unordered_map<uint32_t, Glyph> glyphs; // keyed by glyph index
  };

  SizedFont *sized(uint16_t font_id, int pixel_size);
  Glyph &glyph(SDL_Renderer *renderer, SizedFont &sf, uint32_t glyph_index);

  TTF_Font *faces_[FaceCount] = {};
  std::vector<std::unique_ptr<SizedFont>> sized_; // few dozen; linear scan
  uint64_t rasterizations_ = 0;
};

} // namespace silencer::cppx_ui
