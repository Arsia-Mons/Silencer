#include "../screens.h"

#include "../components/button.h"
#include "../components/panel.h"
#include "../font.h"

namespace silencer {

void ComposeLobbyConnect(Framebuffer &fb, const SpriteSet &sprites,
                         const Palette &palette, int active_sub) {
  // Bank 7 idx 2 panel. Distinct from CONTROLS' idx 7 and LOBBY's idx 1.
  RenderPanel(fb, sprites, {.x = 0, .y = 0, .bank = 7, .idx = 2});

  // Username / Password labels — text-mode at literal screen coords, font
  // bank 134 advance 9.
  DrawText(fb, 190, 291, "Username", /*bank=*/134, /*advance=*/9, sprites,
           palette, active_sub, /*brightness=*/128);
  DrawText(fb, 190, 318, "Password", /*bank=*/134, /*advance=*/9, sprites,
           palette, active_sub, /*brightness=*/128);

  // Login / Cancel B52x21 (text-only) at the bottom-right.
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Login",  .x = 264, .y = 339,
                .variant = ButtonVariant::B52x21});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Cancel", .x = 321, .y = 339,
                .variant = ButtonVariant::B52x21});
}

}  // namespace silencer
