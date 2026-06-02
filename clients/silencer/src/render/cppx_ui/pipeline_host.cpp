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
  // The chrome textures are bound to the old renderer; drop them and flag a
  // re-bake. The composition root re-bakes once after this ensure() returns.
  textures_.shutdown();
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
    install_text_measurer(&fonts_);
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

const uint8_t *PipelineHost::render(const client::ui::UiPipelineFrame &frame,
                                    int *out_w, int *out_h) {
  if (!surf_ || !r_ || !pipeline_)
    return nullptr;

  // Transparent clear: the UI composites over the already-rendered world frame.
  const float scale = ui_.begin_frame(::ui::Color{0, 0, 0, 0}, 1.0f, 1);
  pipeline_->render_client_ui_frame(frame, [&] {
    const ::ui::DrawCommandList &list =
        pipeline_->client_ui().retained_command_list();
    execute_draw_commands(r_, list, &fonts_, &textures_, scale, {});
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
