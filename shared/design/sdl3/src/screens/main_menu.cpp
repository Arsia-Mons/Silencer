#include "../screens.h"

#include <string>

#include "../components/button.h"
#include "../components/panel.h"
#include "../font.h"
#include "../palette.h"
#include "../sprite.h"

namespace silencer {

namespace {

// Logo overlay tick simulation. We just need to reach res_index = 60 (the
// steady-state hold frame) per tick.md / widget-overlay.md.
int LogoTickToHold() {
  int state_i = 0;
  int res_index = 29;
  while (state_i < 90) {
    if (state_i < 60) {
      res_index = state_i / 2 + 29;
    } else if (state_i < 120) {
      res_index = 60;
    } else {
      res_index = 60;
    }
    if (res_index > 60) res_index = 60;
    if (res_index < 29) res_index = 29;
    ++state_i;
  }
  return res_index;
}

}  // namespace

void ComposeMainMenu(Framebuffer &fb, const SpriteSet &sprites,
                     const Palette &palette, int active_sub) {
  RenderPanel(fb, sprites, {.x = 0, .y = 0, .bank = 6, .idx = 0});
  RenderPanel(fb, sprites,
              {.x = 0, .y = 0, .bank = 208, .idx = LogoTickToHold()});

  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Tutorial",         .x = 40, .y = -134,
                .variant = ButtonVariant::B196x33});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Connect To Lobby", .x = 80, .y =  -67,
                .variant = ButtonVariant::B196x33});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Options",          .x = 40, .y =    0,
                .variant = ButtonVariant::B196x33});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Exit",             .x =  0, .y =   67,
                .variant = ButtonVariant::B196x33});

  DrawText(fb, 10, 463, "Silencer v00028", /*bank=*/133, /*advance=*/11,
           sprites, palette, active_sub, /*brightness=*/128);
}

}  // namespace silencer
