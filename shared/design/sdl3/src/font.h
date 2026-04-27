#pragma once

#include <string>

#include "palette.h"
#include "sprite.h"

namespace silencer {

// Draw text onto fb at screen-space (x, y) using the specified font bank
// and per-glyph advance. brightness != 128 ramps glyph pixels through a
// brightness LUT computed against the active sub-palette.
void DrawText(Framebuffer &fb, int x, int y, const std::string &text,
              int bank, int advance, const SpriteSet &sprites,
              const Palette &palette, int active_sub, int brightness);

}  // namespace silencer
