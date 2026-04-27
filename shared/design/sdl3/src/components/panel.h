#pragma once

#include "../sprite.h"

namespace silencer {

// A panel is a single sprite blit at (x, y). Used for fullscreen
// background plates (bank 6 idx 0 — main menu / options) and for
// chromed lobby/options frames (bank 7 idx 1, 2, 7).
struct PanelView {
  int x;
  int y;
  int bank;
  int idx;
};

void RenderPanel(Framebuffer &fb, const SpriteSet &sprites,
                 const PanelView &view);

}  // namespace silencer
