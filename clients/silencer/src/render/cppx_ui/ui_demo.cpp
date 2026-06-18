#include "ui_demo.h"

#include "draw_executor.h"
#include "ui/runtime/draw_command.h"

#ifndef SILENCER_HEADLESS
#include <SDL3_ttf/SDL_ttf.h>
#endif

#include <string.h>

namespace silencer::cppx_ui {

UiDemoOverlay::~UiDemoOverlay() {
  ui_.shutdown();
  fonts_.shutdown();
  if (r_) SDL_DestroyRenderer(r_);
  if (surf_) SDL_DestroySurface(surf_);
}

bool UiDemoOverlay::ensure(int w, int h, const char *font_dir) {
  if (surf_ && r_ && w_ == w && h_ == h)
    return true;

  ui_.shutdown();
  if (r_) { SDL_DestroyRenderer(r_); r_ = nullptr; }
  if (surf_) { SDL_DestroySurface(surf_); surf_ = nullptr; }
  button_tex_ = 0; // textures live on the renderer; rebuilt below

  surf_ = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
  if (!surf_) return false;
  r_ = SDL_CreateSoftwareRenderer(surf_);
  if (!r_) return false;

  if (!fonts_.loaded()) {
#ifndef SILENCER_HEADLESS
    if (!TTF_WasInit()) TTF_Init();
#endif
    fonts_.load_faces(font_dir);
  }
  if (!ui_.initialize(r_, fonts_))
    return false;

  // Synthetic green-bordered nine-slice button: 16x16 with a 3px opaque green
  // border and a translucent dark-green fill (straight alpha; upload_rgba
  // premultiplies). 4px caps leave a real stretchable middle.
  uint8_t t[16 * 16 * 4];
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) {
      uint8_t *px = t + (y * 16 + x) * 4;
      const bool border = (x < 3 || x >= 13 || y < 3 || y >= 13);
      if (border) { px[0] = 60; px[1] = 230; px[2] = 110; px[3] = 255; }
      else        { px[0] = 8;  px[1] = 34;  px[2] = 16;  px[3] = 220; }
    }
  button_tex_ = textures_.upload_rgba(r_, t, 16, 16);

  w_ = w;
  h_ = h;
  packed_.assign((size_t)w * h * 4u, 0);
  return true;
}

static ui::DrawCommandList g_demo_list;

const uint8_t *UiDemoOverlay::render(int *out_w, int *out_h) {
  if (!surf_ || !r_) return nullptr;

  const float scale = ui_.begin_frame(ui::Color{0, 0, 0, 0}, 1.0f, 1); // transparent
  g_demo_list.reset();

  const float bx = w_ * 0.34f, by = h_ * 0.44f, bw = w_ * 0.30f, bh = 64.f;

  // Nine-slice button background.
  ui::DrawCommand btn{
      .kind = ui::DrawCommandKind::Image,
      .rect = {bx, by, bw, bh},
      .payload = {.image = {.texture_id = button_tex_,
                            .tint = {255, 255, 255, 255},
                            .nine_slice = {4.f, 4.f, 4.f, 4.f}}}};
  g_demo_list.push(btn);

  // Button label (body face).
  static const char *kLabel = "cppx button + TTF";
  uint32_t loff = 0;
  g_demo_list.push_text(kLabel, (uint16_t)strlen(kLabel), &loff);
  ui::DrawCommand lbl{
      .kind = ui::DrawCommandKind::Text,
      .rect = {bx + 28.f, by + 20.f, bw, bh},
      .payload = {.text = {.text_off = loff,
                           .text_len = (uint16_t)strlen(kLabel),
                           .color = {150, 255, 180, 255},
                           .font_id = FontRegistry::Body,
                           .font_size = 22}}};
  g_demo_list.push(lbl);

  // Title (title face) above the button.
  static const char *kTitle = "SIL-11 cppx UI bridge";
  uint32_t toff = 0;
  g_demo_list.push_text(kTitle, (uint16_t)strlen(kTitle), &toff);
  ui::DrawCommand title{
      .kind = ui::DrawCommandKind::Text,
      .rect = {bx, by - 56.f, bw * 2.f, 48.f},
      .payload = {.text = {.text_off = toff,
                           .text_len = (uint16_t)strlen(kTitle),
                           .color = {210, 255, 220, 255},
                           .font_id = FontRegistry::Title,
                           .font_size = 30}}};
  g_demo_list.push(title);

  execute_draw_commands(r_, g_demo_list, &fonts_, &textures_, scale, {});
  ui_.resolve_frame();
  ui_.present();

  // Copy to a tightly-packed buffer (the surface pitch may be padded).
  const int pitch = surf_->pitch;
  const uint8_t *src = (const uint8_t *)surf_->pixels;
  for (int y = 0; y < h_; ++y)
    memcpy(&packed_[(size_t)y * w_ * 4u], src + (size_t)y * pitch,
           (size_t)w_ * 4u);

  if (out_w) *out_w = w_;
  if (out_h) *out_h = h_;
  return packed_.data();
}

} // namespace silencer::cppx_ui
