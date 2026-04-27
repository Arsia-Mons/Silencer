#include "header.h"

#include "../font.h"
#include "button.h"

namespace silencer {

void RenderHeader(Framebuffer &fb, const SpriteSet &sprites,
                  const Palette &palette, int active_sub,
                  const HeaderView &view) {
  // Title overlay — text-mode at literal screen coords (no anchor offset).
  if (!view.title.empty()) {
    DrawText(fb, 15, 32, view.title, /*bank=*/135, /*advance=*/11, sprites,
             palette, active_sub, /*brightness=*/128);
  }
  if (!view.version.empty()) {
    DrawText(fb, 115, 39, "v." + view.version, /*bank=*/133, /*advance=*/6,
             sprites, palette, active_sub, /*brightness=*/128);
  }
  if (view.show_back_button) {
    RenderButton(fb, sprites, palette, active_sub,
                 {.label = "Go Back", .x = 473, .y = 29,
                  .variant = ButtonVariant::B156x21});
  }
}

}  // namespace silencer
