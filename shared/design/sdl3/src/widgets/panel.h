// Horizontal-stretch panel (sprite bank 188): top/bottom rows only.
#pragma once

#include <cstdint>

namespace silencer {

class SpriteBanks;

void DrawHStretchPanel(std::uint8_t* dst, int dst_w, int dst_h,
                       int x, int y, int w, int h,
                       const SpriteBanks& banks);

}  // namespace silencer
