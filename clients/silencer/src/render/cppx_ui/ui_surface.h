#pragma once

#include <SDL3/SDL.h>

#include "ui/style/visual_style.h" // ::ui::Color (src-relative; lives under src/render/cppx_ui)
#include "font_registry.h"
#include "sdf_raster.h"

namespace silencer::cppx_ui {

// Minimal SDL surface the per-frame loop draws into: clear -> execute the
// DrawCommand IR via execute_draw_commands -> present. Owns no draw IR itself
// (the new tagged-union IR lives in client::ui); it only exposes the SDL_Renderer
// and the FontRegistry the executor needs. Replaces the deleted
// SdlRetainedRenderer (legacy DrawList path) — renderer/ stays free of
// client/game deps.
class UiSurface {
public:
  ~UiSurface();
  bool initialize(SDL_Renderer *renderer, FontRegistry &fonts);
  // Frees the supersample target (if any) and clears handles. Idempotent. Call
  // before the SDL_Renderer is destroyed (App::shutdown does this); the
  // destructor is a backstop.
  void shutdown();

  void clear(::ui::Color background);
  void present();

  // Begin a (possibly supersampled) frame. When supersample > 1 a cached
  // offscreen target sized output*supersample is bound and cleared to
  // `background`; otherwise the window itself is cleared. Returns the effective
  // device scale to hand to execute_draw_commands (device_scale * supersample
  // when supersampling, else device_scale). Pair every call with resolve_frame.
  // This is full-scene SSAA: render the whole UI at N× then box-downsample on
  // resolve, which anti-aliases everything (curves, gradients, text) on top of
  // the per-primitive feather — crisp edges even on a standard-density display.
  float begin_frame(::ui::Color background, float device_scale, int supersample);
  // Resolve a supersampled frame: unbind the offscreen target and downsample it
  // onto the window with a linear filter. No-op when begin_frame rendered
  // straight to the window. After this the window holds the final frame.
  void resolve_frame();

  SDL_Renderer *sdl_renderer() const { return renderer_; }
  // The FontRegistry handed to initialize(); the draw executor needs it to
  // rasterize text. May be null before initialize().
  FontRegistry *fonts() const { return fonts_; }
  // Persistent SDF coverage-mask cache for RenderMode::Sdf. Owned here (the
  // renderer frame boundary) and freed in shutdown() before the SDL_Renderer is
  // destroyed. Handed to execute_draw_commands each frame.
  SdfMaskCache *sdf_cache() { return &sdf_cache_; }

private:
  SDL_Renderer *renderer_ = nullptr;
  FontRegistry *fonts_ = nullptr;
  SDL_Texture *ss_target_ = nullptr; // cached supersample target (lazily sized)
  int ss_w_ = 0;
  int ss_h_ = 0;
  bool ss_active_ = false; // current frame is rendering into ss_target_
  SdfMaskCache sdf_cache_;
};

} // namespace silencer::cppx_ui
