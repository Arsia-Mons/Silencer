#pragma once

#include <SDL3/SDL.h>

#include "client/ui/app_shell/ui_pipeline.h"
#include "font_registry.h"
#include "glyph_fonts.h"
#include "texture_registry.h"
#include "ui_surface.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace silencer::cppx_ui {

// The production render seam SIL-13 deferred: drives the golden
// client::ui::UiPipeline and turns one frame into a tightly-packed RGBA buffer
// that the GPU backend uploads via RenderDevice::UploadUiFrame (the same
// software-raster -> upload -> GPU-composite path SIL-11 proved with the demo
// overlay, now fed by the real pipeline instead of a hand-built command list).
//
// Each frame: begin the UiSurface, run render_client_ui_frame() — which builds
// the retained tree, lays it out, focuses, and builds the tagged-union IR, then
// calls our render lambda that executes that IR via execute_draw_commands —
// resolve, and copy the surface to a packed buffer. SDL-side; lives in
// renderer/ alongside the executor it drives.
//
// react_init_runtime() must have been called once before use (the host does not
// own the global hook runtime). Caller sets frame.layout to ensure()'s w x h.
class PipelineHost {
public:
  PipelineHost();
  ~PipelineHost();

  PipelineHost(const PipelineHost &) = delete;
  PipelineHost &operator=(const PipelineHost &) = delete;

  // (Re)create the w x h software surface + renderer + pipeline. Loads the four
  // UI faces from `font_dir` on first call (the UiSurface requires a default
  // font) and installs the text measurer. Idempotent for an unchanged size.
  // Returns false if SDL surface/renderer creation or font load fails.
  bool ensure(int w, int h, const char *font_dir);

  client::ui::UiPipeline &pipeline() { return *pipeline_; }

  // Run one pipeline frame for `frame` and return tightly-packed RGBA (w*h*4),
  // or null if not initialized. The buffer is owned by the host and valid until
  // the next render()/ensure().
  const uint8_t *render(const client::ui::UiPipelineFrame &frame, int *out_w,
                        int *out_h);

  // ---- Chrome sprite bake seam (SIL-87) ----------------------------------
  // The SDL_Textures are bound to the software renderer `r_`, which ensure()
  // recreates on a size change — so baked chrome must be re-baked whenever `r_`
  // is recreated (tracked by `chrome_dirty_`), NOT every frame. The composition
  // root (src/game/ui) owns WHICH legacy sprites to bake (it has Game/Surface/
  // Palette); this host just bakes arbitrary indexed pixels into a texture_id.
  //
  // Usage per frame, after ensure():
  //   if (host.chrome_needs_bake()) { /* bake_chrome_sprite()… */ host.mark_chrome_baked(); }
  bool chrome_needs_bake() const { return chrome_dirty_; }

  // Bake w*h palette indices (row-major) + a 256-entry palette into a
  // premultiplied-RGBA texture owned by this host's registry; returns its
  // texture_id (0 on failure / not initialized / registry full).
  uint32_t bake_chrome_sprite(const uint8_t *indices, int w, int h,
                              const SDL_Color *palette256);

  // Build one bitmap glyph FACE atlas (origin/main text parity). `glyphs[i]` is
  // the source sprite for char GlyphFonts::kFirstChar + i (null/empty = blank
  // cell). advance/line_height are the native (640-space) bank metrics. Baked
  // into a host-owned atlas texture re-created on each renderer reset (re-baked
  // in the same pass as the chrome). Returns false on failure.
  bool build_glyph_face(int face_id, const GlyphFonts::GlyphSrc *glyphs,
                        int count, const SDL_Color *palette256, float advance,
                        float line_height);

  // Exact-color glyph variant (see GlyphFonts::build_color_face): the caller
  // pre-applies the legacy Effect* pipeline to the indexed art; the variant is
  // selected at draw time when a Text command carries the key color.
  bool build_glyph_color_face(int face_id, uint8_t key_r, uint8_t key_g,
                              uint8_t key_b, const GlyphFonts::GlyphSrc *glyphs,
                              int count, const SDL_Color *palette256,
                              float advance, float line_height);

  void mark_chrome_baked() { chrome_dirty_ = false; }

private:
  SDL_Surface *surf_ = nullptr;
  SDL_Renderer *r_ = nullptr;
  FontRegistry fonts_;
  GlyphFonts glyph_fonts_;
  TextureRegistry textures_;
  UiSurface ui_;
  std::unique_ptr<client::ui::UiPipeline> pipeline_;
  std::vector<uint8_t> packed_;
  std::vector<uint8_t> bake_scratch_;
  int w_ = 0;
  int h_ = 0;
  bool chrome_dirty_ = true; // re-bake chrome when r_ (and its textures) reset
};

} // namespace silencer::cppx_ui
