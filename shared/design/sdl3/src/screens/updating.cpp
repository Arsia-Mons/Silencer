#include "../screens.h"

#include <cstdint>
#include <cstring>

#include "../components/button.h"
#include "../font.h"

namespace silencer {

void ComposeUpdating(Framebuffer &fb, const SpriteSet &sprites,
                     const Palette &palette, int active_sub) {
  // Bordered box at (159..478) x (193..258) drawn manually — no sprite for
  // the modal-dialog frame is loaded in this hydration. Border colour is
  // bright green (palette idx 220 in sub-palette 1 -> RGB (24,125,20)),
  // matching the reference dump.
  constexpr uint8_t kBorderIdx = 220;
  constexpr int kBoxLeft = 159;
  constexpr int kBoxRight = 478;
  constexpr int kBoxTop = 193;
  constexpr int kBoxBottom = 258;
  auto plot = [&](int x, int y) {
    if (x < 0 || x >= Framebuffer::W || y < 0 || y >= Framebuffer::H) return;
    fb.px[y * Framebuffer::W + x] = kBorderIdx;
  };
  // Top border (y=193..195), bottom border (y=257..258), thick verticals.
  for (int y = kBoxTop; y <= kBoxTop + 2; ++y)
    for (int x = kBoxLeft; x <= kBoxRight; ++x) plot(x, y);
  for (int y = kBoxBottom - 1; y <= kBoxBottom; ++y)
    for (int x = kBoxLeft; x <= kBoxRight; ++x) plot(x, y);
  for (int y = kBoxTop; y <= kBoxBottom; ++y)
    for (int x = kBoxLeft; x <= kBoxLeft + 5; ++x) plot(x, y);
  for (int y = kBoxTop; y <= kBoxBottom; ++y)
    for (int x = kBoxRight - 5; x <= kBoxRight; ++x) plot(x, y);

  // Status text centered on the box, bank 134 advance 8.
  {
    const char *text = "An update is required to play online.";
    int len = static_cast<int>(std::strlen(text));
    int textX = (kBoxLeft + kBoxRight) / 2 - (len * 8) / 2;
    DrawText(fb, textX, 200, text, /*bank=*/134, /*advance=*/8, sprites,
             palette, active_sub, /*brightness=*/128);
  }

  // Update + Cancel B156x21 buttons filling the box interior side-by-side.
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Update", .x = 161, .y = 230,
                .variant = ButtonVariant::B156x21});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Cancel", .x = 322, .y = 230,
                .variant = ButtonVariant::B156x21});
}

}  // namespace silencer
