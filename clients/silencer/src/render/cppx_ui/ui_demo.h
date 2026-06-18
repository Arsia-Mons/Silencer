#pragma once

// Flag-gated (SILENCER_CPPX_UI_DEMO) end-to-end smoke overlay: a nine-slice
// button + TTF text rendered through the cppx bridge to a packed RGBA buffer.

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

  // (Re)create the w*h surface and load fonts. No-op when already sized.
  bool ensure(int w, int h, const char *font_dir);

  // Render the demo scene; returns packed premultiplied RGBA8 (w*h*4, valid
  // until the next render) or nullptr.
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
