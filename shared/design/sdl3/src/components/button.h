#pragma once

#include <string>

#include "../palette.h"
#include "../sprite.h"

namespace silencer {

// Button variants we render today. Each variant captures the chrome
// sprite (bank/idx) plus the label-font conventions (bank/advance/yoff)
// and the visual width used for label centering. Per
// docs/design-system.md.archive (Button table).
enum class ButtonVariant {
  // Bank 6 idx 7, label font 135 advance 11, yoff 8, width 196.
  // Used by main-menu, OPTIONS hub, OPTIONSAUDIO/DISPLAY/CONTROLS
  // Save/Cancel rows.
  B196x33,
  // Bank 6 idx 23, label font 135 advance 11, yoff 8, width 220.
  // Used by OPTIONSAUDIO/DISPLAY for toggle rows.
  B220x33,
  // Bank 6 idx 28, label font 135 advance 11, yoff 8, width 112.
  // Used by OPTIONSCONTROLS for the keybind action rows.
  B112x33,
  // Bank 7 idx 24, label font 134 advance 8, yoff 4, width 156.
  // Used across the LOBBY family + UPDATING.
  B156x21,
  // No sprite chrome — text-only button. Label font 133 advance 7,
  // yoff 8, width 52, +1 px nudge after centering. Used by
  // LOBBYCONNECT Login/Cancel.
  B52x21,
};

struct ButtonView {
  std::string label;
  int x;
  int y;
  ButtonVariant variant;
  int brightness = 128;  // 128 == INACTIVE / no hover
};

void RenderButton(Framebuffer &fb, const SpriteSet &sprites,
                  const Palette &palette, int active_sub,
                  const ButtonView &view);

}  // namespace silencer
