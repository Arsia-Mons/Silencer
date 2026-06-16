#include "pipeline_host.h"

#include "perf_trace.h"
#include "draw_executor.h"
#include "sprite_bake.h"
#include "text_measure.h"
#include "ui_draw_program_builder.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <cstring>

namespace silencer::cppx_ui {

PipelineHost::PipelineHost() = default;

PipelineHost::~PipelineHost() {
  ui_.shutdown();
  textures_.shutdown();
  glyph_fonts_.shutdown();
  fonts_.shutdown();
  if (r_)
    SDL_DestroyRenderer(r_);
  if (surf_)
    SDL_DestroySurface(surf_);
}

bool PipelineHost::ensure(int w, int h, const char *font_dir) {
  if (w < 1 || h < 1)
    return false;
  if (surf_ && r_ && pipeline_ && w_ == w && h_ == h)
    return true;

  ui_.shutdown();
  // The chrome + glyph textures are bound to the old renderer; drop them and
  // flag a re-bake. The composition root re-bakes once after ensure() returns.
  textures_.shutdown();
  glyph_fonts_.shutdown();
  chrome_dirty_ = true;
  // SIL-240: the registries' textures are gone; bump the generation so the GPU
  // backend flushes its texture cache (texture_ids may be reused for new art).
  ++texture_generation_;
  if (r_) {
    SDL_DestroyRenderer(r_);
    r_ = nullptr;
  }
  if (surf_) {
    SDL_DestroySurface(surf_);
    surf_ = nullptr;
  }

  surf_ = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
  if (!surf_)
    return false;
  r_ = SDL_CreateSoftwareRenderer(surf_);
  if (!r_)
    return false;

  if (!fonts_.loaded()) {
    if (!TTF_WasInit())
      TTF_Init();
    if (!fonts_.load_faces(font_dir))
      return false;
    // Measure reads the glyph fonts first (bitmap parity), TTF as fallback.
    install_text_measurer(&fonts_, &glyph_fonts_);
  }
  if (!ui_.initialize(r_, fonts_))
    return false;
  if (!pipeline_)
    pipeline_ = std::make_unique<client::ui::UiPipeline>();

  w_ = w;
  h_ = h;
  packed_.assign((size_t)w * h * 4u, 0);
  // The surface was just (re)created at a new size; any cached frame is stale.
  packed_valid_ = false;
  last_ir_sig_ = 0;
  return true;
}

uint32_t PipelineHost::bake_chrome_sprite(const uint8_t *indices, int w, int h,
                                          const SDL_Color *palette256) {
  if (!r_ || !indices || !palette256 || w < 1 || h < 1)
    return 0;
  bake_scratch_.assign((size_t)w * h * 4u, 0);
  bake_indexed_rgba(indices, w, h, palette256, bake_scratch_.data());
  return textures_.upload_rgba(r_, bake_scratch_.data(), w, h);
}

void PipelineHost::register_legacy(uint32_t texture_id,
                                   const uint8_t *indices, int w, int h,
                                   const SDL_Color *palette256, int legacy_w,
                                   int legacy_h, LegacyFit fit, int cap_l,
                                   int cap_r, int cap_t, int cap_b) {
  textures_.register_legacy(texture_id, indices, w, h, palette256, legacy_w,
                            legacy_h, fit, cap_l, cap_r, cap_t, cap_b);
}

bool PipelineHost::build_glyph_face(int face_id,
                                    const GlyphFonts::GlyphSrc *glyphs,
                                    int count, const SDL_Color *palette256,
                                    float advance, float line_height) {
  if (!r_)
    return false;
  return glyph_fonts_.build_face(r_, face_id, glyphs, count, palette256, advance,
                                 line_height);
}

bool PipelineHost::build_glyph_color_face(int face_id, uint8_t key_r,
                                          uint8_t key_g, uint8_t key_b,
                                          const GlyphFonts::GlyphSrc *glyphs,
                                          int count,
                                          const SDL_Color *palette256,
                                          float advance, float line_height,
                                          uint8_t alpha) {
  if (!r_)
    return false;
  return glyph_fonts_.build_color_face(r_, face_id, key_r, key_g, key_b, glyphs,
                                       count, palette256, advance, line_height,
                                       alpha);
}

namespace {

// FNV-1a over the LIVE bytes of the draw IR (the used prefix of each arena, not
// the whole fixed-capacity arrays). Trivially-copyable POD, so a raw-byte hash
// captures every visible change — colors, geometry, text, gradients. A false
// "changed" only costs a redundant raster; a collision-driven false "unchanged"
// is astronomically unlikely (and would have to collide on a real visual delta).
uint64_t ir_signature(const ::ui::DrawCommandList &list) {
  uint64_t h = 1469598103934665603ull;
  auto mix_bytes = [&](const void *p, size_t n) {
    const uint8_t *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; ++i) {
      h ^= b[i];
      h *= 1099511628211ull;
    }
  };
  const int count = list.count;
  mix_bytes(&count, sizeof(count));
  mix_bytes(list.commands, (size_t)(count > 0 ? count : 0) * sizeof(list.commands[0]));
  const int text_len = list.text_len_used;
  mix_bytes(&text_len, sizeof(text_len));
  mix_bytes(list.text_arena, (size_t)(text_len > 0 ? text_len : 0));
  const int grad = list.grad_count;
  mix_bytes(&grad, sizeof(grad));
  mix_bytes(list.grad_arena, (size_t)(grad > 0 ? grad : 0) * sizeof(list.grad_arena[0]));
  return h;
}

} // namespace

const uint8_t *PipelineHost::render(const client::ui::UiPipelineFrame &frame,
                                    int *out_w, int *out_h, bool *out_unchanged) {
  if (out_unchanged)
    *out_unchanged = false;
  if (!surf_ || !r_ || !pipeline_)
    return nullptr;

  // Responsive content scale: screens lay out against a logical canvas (height
  // pinned to 720, width = aspect*720) and the executor scales every primitive
  // up to the physical surface, so the whole UI grows with the window like
  // origin/main — fonts re-rasterized crisply at the scaled cell, sprites
  // upscaled. Derived from the surface/logical-layout ratio the composition root
  // set in frame.layout; 1.0 when the layout already matches the surface.
  // May be < 1 (small windows render the 720-authored canvas at origin's
  // native 480-virtual proportions) — keep it in lockstep with the
  // composition root's cppxScale, never re-clamp here.
  float device_scale = (frame.layout.height > 0.0f)
                           ? static_cast<float>(h_) / frame.layout.height
                           : 1.0f;
  // SIL-237: the retained tree, layout, focus pass, and IR build always run —
  // they advance the animation/interaction state. Only the native-resolution
  // raster is conditional. `unchanged` is decided AFTER the IR is built (inside
  // the render callback): if the IR is byte-identical to the last rastered
  // frame and we hold a valid cached buffer, skip the clear/execute/SSAA-resolve
  // /packed-copy entirely and reuse `packed_`.
  bool unchanged = false;
  pipeline_->render_client_ui_frame(frame, [&] {
    const ::ui::DrawCommandList &list =
        pipeline_->client_ui().retained_command_list();
    const uint64_t sig = ir_signature(list);
    if (packed_valid_ && sig == last_ir_sig_) {
      unchanged = true;
      return; // cached packed_ is still correct for this IR
    }
    PERF_SCOPE("ui.raster");
    // Transparent clear: the UI composites over the already-rendered world.
    const float scale =
        ui_.begin_frame(::ui::Color{0, 0, 0, 0}, device_scale, 1);
    execute_draw_commands(r_, list, &fonts_, &textures_, scale, {},
                          &glyph_fonts_);
    ui_.resolve_frame();
    ui_.present();
    // Copy to a tightly-packed buffer (the surface pitch may be padded).
    const int pitch = surf_->pitch;
    const uint8_t *src = (const uint8_t *)surf_->pixels;
    for (int y = 0; y < h_; ++y)
      memcpy(&packed_[(size_t)y * w_ * 4u], src + (size_t)y * pitch,
             (size_t)w_ * 4u);
    last_ir_sig_ = sig;
    packed_valid_ = true;
  });

  if (out_unchanged)
    *out_unchanged = unchanged;
  if (out_w)
    *out_w = w_;
  if (out_h)
    *out_h = h_;
  return packed_.data();
}

const GpuUiProgram *
PipelineHost::build_gpu_frame(const client::ui::UiPipelineFrame &frame) {
  if (!surf_ || !r_ || !pipeline_)
    return nullptr;

  // Same responsive content scale render() derives (production supersample is 1,
  // so the executor's scale == this device_scale).
  const float device_scale = (frame.layout.height > 0.0f)
                                 ? static_cast<float>(h_) / frame.layout.height
                                 : 1.0f;

  // The retained tree, layout, focus pass, and IR build always run (they carry
  // the animation/interaction state); only the lowering target differs from
  // render() — geometry instead of a CPU raster.
  pipeline_->render_client_ui_frame(frame, [&] {
    const ::ui::DrawCommandList &list =
        pipeline_->client_ui().retained_command_list();
    PERF_SCOPE("ui.gpu_build");
    build_ui_draw_program(list, &fonts_, &textures_, &glyph_fonts_, r_,
                          device_scale, w_, h_, texture_generation_,
                          gpu_program_);
  });
  return &gpu_program_;
}

} // namespace silencer::cppx_ui
