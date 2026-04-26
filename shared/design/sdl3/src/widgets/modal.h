// Modal dialog — sprite bank 40 idx 4 background, message + optional OK.
#pragma once

#include <cstdint>
#include <string>

namespace silencer {

class SpriteBanks;
class Palette;
class Button;

void DrawModal(std::uint8_t* dst, int dst_w, int dst_h,
               const std::string& message,
               bool ok_button,
               const SpriteBanks& banks, const Palette& pal);

}  // namespace silencer
