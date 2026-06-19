#pragma once

// SIL-11 end-to-end demo overlay. Renders a nine-slice button + TTF text to a
// window-sized premultiplied-RGBA buffer through the cppx bridge (UiSurface ->
// draw_executor), which the GPU backend composites over the frame. Flag-gated
// (SILENCER_CPPX_UI_DEMO) — proves the whole bridge on the real screen; the
// pipeline-driven UI (cppx screens) is SIL-13.

#include "font_registry.h"
#include "texture_registry.h"
#include "ui_surface.h"

#include <SDL3/SDL.h>

#include <stdint.h>
#include <vector>

namespace silencer::cppx_ui {

class UiDemoOverlay {
public:
  ~UiDemoOverlay();

  // (Re)create the software-rendered surface at w*h and load fonts from
  // `font_dir`. Cheap no-op when already sized.
  bool ensure(int w, int h, const char *font_dir);

  // Render the demo scene; returns a tightly-packed premultiplied-RGBA8 buffer
  // (w*h*4, valid until the next render) or nullptr.
  const uint8_t *render(int *out_w, int *out_h);

private:
  SDL_Surface  *surf_ = nullptr;
  SDL_Renderer *r_ = nullptr;
  FontRegistry  fonts_;
  TextureRegistry textures_;
  UiSurface     ui_;
  uint32_t      button_tex_ = 0;
  int           w_ = 0;
  int           h_ = 0;
  std::vector<uint8_t> packed_; // tight copy (surface pitch may be padded)
};

} // namespace silencer::cppx_ui
