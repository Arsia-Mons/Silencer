#pragma once

#include <string>

#include "../palette.h"
#include "../sprite.h"

namespace silencer {

// LOBBY-family header bar: title at top-left, version readout next to it,
// optional B156x21 "Go Back" button at top-right. Shared by LOBBY +
// LOBBY GameCreate/Join/Tech modals.
struct HeaderView {
  std::string title;       // e.g. "Silencer", drawn at (15, 32) bank 135 advance 11
  std::string version;     // e.g. "00028" — prefixed with "v." and drawn at (115, 39) bank 133 advance 6
  bool show_back_button;   // when true, B156x21 "Go Back" at (473, 29)
};

void RenderHeader(Framebuffer &fb, const SpriteSet &sprites,
                  const Palette &palette, int active_sub,
                  const HeaderView &view);

}  // namespace silencer
