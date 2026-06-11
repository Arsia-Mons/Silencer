#include "pipeline_host.h"

#include "draw_executor.h"
#include "sprite_bake.h"
#include "text_measure.h"

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

uint32_t PipelineHost::bake_backdrop_sprite(const uint8_t *indices, int w,
                                            int h, const SDL_Color *palette256,
                                            bool stretch, int legacy_w,
                                            int legacy_h) {
  if (!r_ || !indices || !palette256 || w < 1 || h < 1 || w_ < 1 || h_ < 1 ||
      legacy_w < 1 || legacy_h < 1)
    return 0;
  bake_scratch_.assign((size_t)w_ * h_ * 4u, 0);
  bake_backdrop_rgba(indices, w, h, palette256, stretch, legacy_w, legacy_h,
                     w_, h_, bake_scratch_.data());
  return textures_.upload_rgba(r_, bake_scratch_.data(), w_, h_);
}

uint32_t PipelineHost::bake_element_sprite(const uint8_t *indices, int w,
                                           int h, const SDL_Color *palette256,
                                           int bx, int by, int bw, int bh,
                                           int legacy_w, int legacy_h,
                                           int dev_x, int dev_y, int tex_w,
                                           int tex_h) {
  if (!r_ || !indices || !palette256 || w < 1 || h < 1 || w_ < 1 || h_ < 1 ||
      legacy_w < 1 || legacy_h < 1 || bw < 1 || bh < 1 || tex_w < 1 ||
      tex_h < 1)
    return 0;
  bake_scratch_.assign((size_t)tex_w * tex_h * 4u, 0);
  bake_element_rgba(indices, w, h, palette256, bx, by, bw, bh, legacy_w,
                    legacy_h, w_, h_, dev_x, dev_y, tex_w, tex_h,
                    bake_scratch_.data());
  return textures_.upload_rgba(r_, bake_scratch_.data(), tex_w, tex_h);
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
                                          int legacy_w, int legacy_h) {
  if (!r_)
    return false;
  return glyph_fonts_.build_color_face(r_, face_id, key_r, key_g, key_b, glyphs,
                                       count, palette256, advance, line_height,
                                       legacy_w, legacy_h);
}

const uint8_t *PipelineHost::render(const client::ui::UiPipelineFrame &frame,
                                    int *out_w, int *out_h) {
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
  // Transparent clear: the UI composites over the already-rendered world frame.
  const float scale = ui_.begin_frame(::ui::Color{0, 0, 0, 0}, device_scale, 1);
  pipeline_->render_client_ui_frame(frame, [&] {
    const ::ui::DrawCommandList &list =
        pipeline_->client_ui().retained_command_list();
    execute_draw_commands(r_, list, &fonts_, &textures_, scale, {},
                          &glyph_fonts_);
  });
  ui_.resolve_frame();
  ui_.present();

  // Copy to a tightly-packed buffer (the surface pitch may be padded).
  const int pitch = surf_->pitch;
  const uint8_t *src = (const uint8_t *)surf_->pixels;
  for (int y = 0; y < h_; ++y)
    memcpy(&packed_[(size_t)y * w_ * 4u], src + (size_t)y * pitch,
           (size_t)w_ * 4u);

  if (out_w)
    *out_w = w_;
  if (out_h)
    *out_h = h_;
  return packed_.data();
}

} // namespace silencer::cppx_ui
