#include "font.h"

#include <array>

#include "palette.h"

namespace silencer {

void DrawText(uint8_t *fb, int fb_w, int fb_h,
              int x, int y,
              const std::string &text,
              Sprites &sprites,
              int bank, int advance,
              bool /*alpha*/,
              uint8_t color,
              uint8_t brightness,
              const Palette &palette) {
    int ioffset = (bank == 132) ? 34 : 33;
    const SpriteBank &b = sprites.Bank(bank);

    // Build brightness LUT once if needed.
    std::array<uint8_t, 256> lut{};
    bool use_lut = false;
    if (brightness != 128) {
        palette.BuildBrightnessLut(static_cast<int>(brightness), lut);
        use_lut = true;
    }
    (void)color;  // not used by the menu (color always 0)

    int xc = 0;
    for (unsigned char ch : text) {
        if (ch == ' ' || ch == 0xA0) {
            xc += advance;
            continue;
        }
        int gi = static_cast<int>(ch) - ioffset;
        if (gi < 0 || gi >= b.count) {
            xc += advance;
            continue;
        }
        const Sprite &g = b.sprites[gi];
        // Glyph blit at (x + xc, y), with sprite anchor offsets applied.
        int top_left_x = x + xc - g.offset_x;
        int top_left_y = y - g.offset_y;
        Sprites::Blit(fb, fb_w, fb_h, g, top_left_x, top_left_y,
                      use_lut ? lut.data() : nullptr);
        xc += advance;
    }
}

}  // namespace silencer
