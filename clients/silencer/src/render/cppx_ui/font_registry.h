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
  // 3=tiny (HUD digits). Matches doc §SIL-6 decision 1's face wiring.
  enum FaceId : uint16_t { Body = 0, Large = 1, Title = 2, Tiny = 3, FaceCount = 4 };

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

private:
  TTF_Font *faces_[FaceCount] = {};
};

} // namespace silencer::cppx_ui
