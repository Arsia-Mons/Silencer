#pragma once
#include <cstdint>
#include <string>

#include "sprite.h"

namespace silencer {

// Draws ASCII text into an indexed framebuffer using a font sprite
// bank. Mirrors Renderer::DrawText.
void DrawText(uint8_t *fb, int fb_w, int fb_h,
              int x, int y,
              const std::string &text,
              Sprites &sprites,
              int bank, int advance,
              bool alpha,         // unused for the menu (no alpha LUT)
              uint8_t color,      // 0 = no tint
              uint8_t brightness, // 128 = neutral
              const class Palette &palette);

}  // namespace silencer
