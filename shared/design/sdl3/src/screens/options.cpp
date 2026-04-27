#include "../screens.h"

#include "../components/button.h"
#include "../components/panel.h"

namespace silencer {

void ComposeOptions(Framebuffer &fb, const SpriteSet &sprites,
                    const Palette &palette, int active_sub) {
  RenderPanel(fb, sprites, {.x = 0, .y = 0, .bank = 6, .idx = 0});

  // Four B196x33 buttons at anchor x=-89, y in {-142, -90, -38, 15}.
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Controls", .x = -89, .y = -142,
                .variant = ButtonVariant::B196x33});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Display",  .x = -89, .y =  -90,
                .variant = ButtonVariant::B196x33});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Audio",    .x = -89, .y =  -38,
                .variant = ButtonVariant::B196x33});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Go Back",  .x = -89, .y =   15,
                .variant = ButtonVariant::B196x33});
}

}  // namespace silencer
