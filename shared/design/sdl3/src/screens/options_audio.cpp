#include "../screens.h"

#include <string>

#include "../components/button.h"
#include "../components/panel.h"
#include "../font.h"

namespace silencer {

namespace {

void DrawCenteredTitle(Framebuffer &fb, const SpriteSet &sprites,
                       const Palette &palette, int active_sub,
                       const std::string &title) {
  constexpr int kAdvance = 12;
  int title_x = 320 - static_cast<int>(title.size()) * kAdvance / 2;
  DrawText(fb, title_x, 14, title, /*bank=*/135, kAdvance, sprites, palette,
           active_sub, /*brightness=*/128);
}

}  // namespace

void ComposeOptionsAudio(Framebuffer &fb, const SpriteSet &sprites,
                         const Palette &palette, int active_sub) {
  RenderPanel(fb, sprites, {.x = 0, .y = 0, .bank = 6, .idx = 0});

  DrawCenteredTitle(fb, sprites, palette, active_sub, "Audio Options");

  // Music toggle button.
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Music", .x = 100, .y = 50,
                .variant = ButtonVariant::B220x33});

  // Off / On half-pill indicators (literal screen coords). Bank 6 idx 12 =
  // off-left (dim outline), idx 15 = on-right (lit fill). The
  // screen-options-audio spec lists "on" as idx 14, but the canonical
  // reference dump uses idx 15 — the spec was wrong, the dump is right.
  RenderPanel(fb, sprites, {.x = 420, .y = 137, .bank = 6, .idx = 12});
  RenderPanel(fb, sprites, {.x = 450, .y = 137, .bank = 6, .idx = 15});

  // Save / Cancel.
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Save",   .x = -200, .y = 117,
                .variant = ButtonVariant::B196x33});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Cancel", .x =   20, .y = 117,
                .variant = ButtonVariant::B196x33});
}

}  // namespace silencer
