#include "../screens.h"

#include <array>
#include <string>

#include "../components/button.h"
#include "../components/panel.h"
#include "../font.h"

namespace silencer {

void ComposeOptionsDisplay(Framebuffer &fb, const SpriteSet &sprites,
                           const Palette &palette, int active_sub) {
  RenderPanel(fb, sprites, {.x = 0, .y = 0, .bank = 6, .idx = 0});

  // Title.
  {
    const std::string title = "Display Options";
    constexpr int kAdvance = 12;
    int title_x = 320 - static_cast<int>(title.size()) * kAdvance / 2;
    DrawText(fb, title_x, 14, title, /*bank=*/135, kAdvance, sprites, palette,
             active_sub, /*brightness=*/128);
  }

  // Two B220x33 toggle rows: Fullscreen, Smooth Scaling. Stride +53.
  const std::array<const char *, 2> labels = {{"Fullscreen", "Smooth Scaling"}};
  constexpr int kToggleX = 100;
  constexpr int kToggleY0 = 50;
  constexpr int kRowStride = 53;
  constexpr int kPillY0 = 137;
  for (size_t i = 0; i < labels.size(); ++i) {
    int by = kToggleY0 + static_cast<int>(i) * kRowStride;
    int py = kPillY0 + static_cast<int>(i) * kRowStride;
    RenderButton(fb, sprites, palette, active_sub,
                 {.label = labels[i], .x = kToggleX, .y = by,
                  .variant = ButtonVariant::B220x33});
    RenderPanel(fb, sprites, {.x = 420, .y = py, .bank = 6, .idx = 12});
    RenderPanel(fb, sprites, {.x = 450, .y = py, .bank = 6, .idx = 15});
  }

  // Save / Cancel.
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Save",   .x = -200, .y = 117,
                .variant = ButtonVariant::B196x33});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Cancel", .x =   20, .y = 117,
                .variant = ButtonVariant::B196x33});
}

}  // namespace silencer
