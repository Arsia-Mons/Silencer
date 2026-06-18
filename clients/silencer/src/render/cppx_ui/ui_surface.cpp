#include "ui_surface.h"

namespace silencer::cppx_ui {

UiSurface::~UiSurface() { shutdown(); }

bool UiSurface::initialize(SDL_Renderer *renderer, FontRegistry &fonts) {
  if (!renderer || !fonts.default_font())
    return false;
  renderer_ = renderer;
  fonts_ = &fonts;
  return true;
}

void UiSurface::shutdown() {
  sdf_cache_.clear(); // free SDF mask textures before the renderer dies
  if (ss_target_) {
    SDL_DestroyTexture(ss_target_);
    ss_target_ = nullptr;
  }
  ss_w_ = ss_h_ = 0;
  ss_active_ = false;
  renderer_ = nullptr;
  fonts_ = nullptr;
}

void UiSurface::clear(::ui::Color background) {
  if (!renderer_)
    return;
  SDL_SetRenderDrawColor(renderer_, background.r, background.g, background.b,
                         background.a);
  SDL_RenderClear(renderer_);
}

float UiSurface::begin_frame(::ui::Color background, float device_scale,
                             int supersample) {
  ss_active_ = false;
  if (device_scale <= 0.f)
    device_scale = 1.0f;
  if (!renderer_)
    return device_scale;
  if (supersample < 1)
    supersample = 1;

  if (supersample > 1) {
    int out_w = 0, out_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer_, &out_w, &out_h);
    const long want_w = static_cast<long>(out_w) * supersample;
    const long want_h = static_cast<long>(out_h) * supersample;
    // Defensive cap so a huge window can't request an absurd target.
    constexpr long kMaxDim = 8192;
    if (out_w > 0 && out_h > 0 && want_w <= kMaxDim && want_h <= kMaxDim) {
      if (ss_target_ && (ss_w_ != static_cast<int>(want_w) ||
                         ss_h_ != static_cast<int>(want_h))) {
        SDL_DestroyTexture(ss_target_);
        ss_target_ = nullptr;
      }
      if (!ss_target_) {
        ss_target_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_TARGET,
                                       static_cast<int>(want_w),
                                       static_cast<int>(want_h));
        if (ss_target_) {
          ss_w_ = static_cast<int>(want_w);
          ss_h_ = static_cast<int>(want_h);
          // Linear filter = box-average on the downsample (the actual AA);
          // BLEND_NONE so resolve is an opaque copy over the window.
          SDL_SetTextureScaleMode(ss_target_, SDL_SCALEMODE_LINEAR);
          SDL_SetTextureBlendMode(ss_target_, SDL_BLENDMODE_NONE);
        }
      }
      if (ss_target_) {
        SDL_SetRenderTarget(renderer_, ss_target_);
        SDL_SetRenderDrawColor(renderer_, background.r, background.g,
                               background.b, background.a);
        SDL_RenderClear(renderer_);
        ss_active_ = true;
        return device_scale * static_cast<float>(supersample);
      }
    }
  }

  // Direct path: render straight to the window backbuffer.
  SDL_SetRenderDrawColor(renderer_, background.r, background.g, background.b,
                         background.a);
  SDL_RenderClear(renderer_);
  return device_scale;
}

void UiSurface::resolve_frame() {
  if (!renderer_ || !ss_active_)
    return;
  SDL_SetRenderTarget(renderer_, nullptr); // back to the window
  // Downsample the supersample target onto the whole window (linear filter).
  SDL_RenderTexture(renderer_, ss_target_, nullptr, nullptr);
  ss_active_ = false;
}

void UiSurface::present() {
  if (renderer_)
    SDL_RenderPresent(renderer_);
}

} // namespace silencer::cppx_ui
