#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace silencer {

// 11 sub-palettes, 256 entries each, RGB888 (channels expanded from 6-bit).
struct Palette {
  // palettes[sub][index] = (r, g, b)
  std::array<std::array<std::array<uint8_t, 3>, 256>, 11> palettes{};

  // Load PALETTE.BIN per spec: offset(s) = 4 + s * (768 + 4). The file is
  // only 8448 bytes, so reads past EOF leave bytes zero (do NOT collapse
  // the layout to contiguous 11 * 768).
  bool LoadFromFile(const std::string &path);

  // Build a brightness LUT for the given sub-palette. Indices 0 and 1 are
  // protected (transparent + black) and pass through unchanged.
  // brightness == 128 produces identity.
  std::array<uint8_t, 256> BuildBrightnessLUT(int sub_palette,
                                              int brightness) const;
};

}  // namespace silencer
