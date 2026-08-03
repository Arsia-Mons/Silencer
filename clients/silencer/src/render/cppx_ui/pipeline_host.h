#pragma once

#include <SDL3/SDL.h>

#include "client/ui/app_shell/ui_pipeline.h"
#include "font_registry.h"
#include "glyph_fonts.h"
#include "texture_registry.h"
#include "ui/runtime/draw_command.h"
#include "ui_draw_program.h"
#include "ui_surface.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace silencer::cppx_ui {

// Damage report for one render(): the device-px regions whose pixels differ
// from the previously returned buffer, so callers can upload just those rects.
// full=true => the whole surface re-rastered (first frame, resize, structural
// IR change, or SILENCER_UI_DAMAGE=0); count==0 && !full => nothing changed.
struct UiDamage {
  static constexpr int kMaxRects = 8;
  SDL_Rect rects[kMaxRects] = {};
  int count = 0;
  bool full = false;
};

// Drives the client::ui::UiPipeline for one frame, producing either a packed
// RGBA buffer (CPU path, render()) or a GpuUiProgram (GPU path,
// build_gpu_frame()). SDL-side; owns the SDL-bound registries.
//
// react_init_runtime() must have been called once before use. Caller sets
// frame.layout to ensure()'s w x h.
class PipelineHost {
public:
  PipelineHost();
  ~PipelineHost();

  PipelineHost(const PipelineHost &) = delete;
  PipelineHost &operator=(const PipelineHost &) = delete;

  // (Re)create the w x h software surface + renderer + pipeline. Loads the UI
  // faces from `font_dir` and installs the text measurer on first call.
  // Idempotent for an unchanged size. False on SDL/font failure.
  bool ensure(int w, int h, const char *font_dir);

  client::ui::UiPipeline &pipeline() { return *pipeline_; }

  // Run one pipeline frame and return host-owned packed RGBA (w*h*4, valid
  // until the next render()/ensure()), or null if not initialized. The raster
  // is damage-tracked: the new IR is diffed per-command against the previous
  // frame's (arena content compared, not offsets) and only the changed regions
  // are cleared, re-executed (clipped), and re-packed — an unchanged IR skips
  // the raster entirely. `out_unchanged` (when non-null) reports the skip so
  // the caller can also skip the GPU upload (the upload additionally depends
  // on the fade alpha, not in the IR — the caller owns that check).
  // `out_damage` (when non-null) reports the repainted rects for partial
  // uploads. SILENCER_UI_DAMAGE=0 forces every repaint to be full-surface
  // (the unchanged skip stays).
  const uint8_t *render(const client::ui::UiPipelineFrame &frame, int *out_w,
                        int *out_h, bool *out_unchanged = nullptr,
                        UiDamage *out_damage = nullptr);

  // GPU path: same build/layout/focus/IR as render(), but lowers the IR to a
  // backend-neutral GpuUiProgram the backend draws directly. Returns the
  // host-owned program (valid until the next build_gpu_frame()/ensure()), or
  // null if not initialized. Windowed; render() stays the headless/test path.
  const GpuUiProgram *build_gpu_frame(const client::ui::UiPipelineFrame &frame);

  // ---- Chrome sprite bake seam ----------------------------------
  // Baked chrome textures are bound to `r_`, which ensure() recreates on a size
  // change, so they must be re-baked whenever `r_` resets (tracked by
  // chrome_dirty_), not every frame. The composition root owns WHICH sprites to
  // bake; this host bakes arbitrary indexed pixels into a texture_id.
  bool chrome_needs_bake() const { return chrome_dirty_; }

  // Bake w*h palette indices + a 256-entry palette into a premultiplied-RGBA
  // texture owned by this host's registry; returns its texture_id (0 on
  // failure / not initialized / registry full).
  uint32_t bake_chrome_sprite(const uint8_t *indices, int w, int h,
                              const SDL_Color *palette256);

  // Register a baked chrome texture's indexed source so the executor can swap
  // qualifying draws for lazily-baked canonical device-cell variants. `fit`
  // picks the box arithmetic; caps (virtual px) apply to NineSlice only.
  void register_legacy(uint32_t texture_id, const uint8_t *indices, int w,
                       int h, const SDL_Color *palette256, int legacy_w,
                       int legacy_h, LegacyFit fit, int cap_l = 0,
                       int cap_r = 0, int cap_t = 0, int cap_b = 0);

  // Build one bitmap glyph face atlas. `glyphs[i]` is the source for char
  // GlyphFonts::kFirstChar + i. advance/line_height are native (640-space).
  // Re-baked on each renderer reset (same pass as chrome). False on failure.
  bool build_glyph_face(int face_id, const GlyphFonts::GlyphSrc *glyphs,
                        int count, const SDL_Color *palette256, float advance,
                        float line_height);

  // Exact-color glyph variant; see GlyphFonts::build_color_face.
  bool build_glyph_color_face(int face_id, uint8_t key_r, uint8_t key_g,
                              uint8_t key_b, const GlyphFonts::GlyphSrc *glyphs,
                              int count, const SDL_Color *palette256,
                              float advance, float line_height,
                              uint8_t alpha = 255);

  void mark_chrome_baked() { chrome_dirty_ = false; }

private:
  // IR diff → damage set (see pipeline_host.cpp for the full-damage triggers).
  void diff_damage(const ::ui::DrawCommandList &list, float scale,
                   UiDamage *out);
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
  // Damage diff state: used prefixes of the last rastered frame's IR (commands
  // + dereferenced arenas) and its raster scale. prev_valid_ also asserts that
  // packed_ holds that frame's pixels; both reset together in ensure().
  std::vector<::ui::DrawCommand> prev_cmds_;
  std::vector<char> prev_text_;
  std::vector<::ui::GradientStop> prev_grads_;
  float prev_scale_ = 0.f;
  bool prev_valid_ = false;
  bool damage_enabled_ = true; // SILENCER_UI_DAMAGE=0 => full repaints only
  GpuUiProgram gpu_program_;
  uint64_t texture_generation_ = 1; // bumps on registry reset (cache flush)
};

} // namespace silencer::cppx_ui
