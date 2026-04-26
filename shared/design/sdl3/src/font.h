// Bitmap font rendering using sprite banks 132–136 (per §Typography).
#pragma once

#include <cstdint>
#include <string_view>

namespace silencer {

class SpriteBanks;
class Palette;

struct DrawTextOpts {
    unsigned bank = 135;        // Font bank (132..136).
    int width = 11;             // Fixed advance per character (px).
    std::uint8_t color = 0;     // EffectColor palette index (0 = no tint).
    std::uint8_t brightness = 128;  // EffectBrightness (128 = neutral).
    bool shadow = false;        // 1 px (+1,+1) shadow at brightness=max(brightness-64, 8).
};

// Returns the ASCII offset for a font bank: 34 for bank 132, 33 otherwise.
inline int FontAsciiOffset(unsigned bank) { return bank == 132 ? 34 : 33; }

// Returns the documented glyph height (used for hit-testing in Overlay text mode).
int FontGlyphHeight(unsigned bank);

// Draws `text` left-edge-anchored at (x, y) into the 8-bit indexed dst surface.
// Uses the SpriteBanks blitter with a tint LUT computed from `color/brightness`
// against the palette. Space (0x20) is skipped (cursor still advances).
void DrawText(std::uint8_t* dst, int dst_w, int dst_h,
              int x, int y, std::string_view text,
              const DrawTextOpts& opts,
              const SpriteBanks& banks, const Palette& pal);

// DrawTinyText (bank 132 / width 4), centered on `x`. Honors the '1' -1px shift.
void DrawTinyText(std::uint8_t* dst, int dst_w, int dst_h,
                  int x, int y, std::string_view text,
                  std::uint8_t color, std::uint8_t brightness,
                  const SpriteBanks& banks, const Palette& pal);

// Convenience: text width in pixels = strlen(text) * width.
inline int TextWidthPx(std::string_view text, int width) {
    return static_cast<int>(text.size()) * width;
}

}  // namespace silencer
