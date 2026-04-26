#include "font.h"

#include <array>
#include <cmath>

#include "palette.h"
#include "sprite.h"

namespace silencer {

namespace {

// Build an index->index lookup that applies EffectColor (if color != 0) then
// EffectBrightness (if brightness != 128), then snaps to the nearest palette 0
// entry by Euclidean distance. Indices 0 and 1 are "protected" per docs.
std::array<std::uint8_t, 256> BuildTintLut(const Palette& pal,
                                           std::uint8_t color,
                                           std::uint8_t brightness) {
    std::array<std::uint8_t, 256> lut{};
    Rgb tint = pal.Color(color);
    bool do_color = (color != 0);
    bool do_bright = (brightness != 128);

    for (int i = 0; i < 256; ++i) {
        if (i < 2) {
            lut[i] = static_cast<std::uint8_t>(i);
            continue;
        }
        Rgb c = pal.Color(i);
        if (do_color) c = Palette::ApplyColorTint(c, tint);
        if (do_bright) c = Palette::ApplyBrightness(c, brightness);
        // Nearest palette index by Euclidean distance, skipping 0 (transparent).
        int best = i;
        long best_d = (1L << 30);
        for (int j = 2; j < 256; ++j) {
            const Rgb& p = pal.Color(j);
            long dr = int(c.r) - int(p.r);
            long dg = int(c.g) - int(p.g);
            long db = int(c.b) - int(p.b);
            long d = dr * dr + dg * dg + db * db;
            if (d < best_d) {
                best_d = d;
                best = j;
                if (d == 0) break;
            }
        }
        lut[i] = static_cast<std::uint8_t>(best);
    }
    return lut;
}

}  // namespace

int FontGlyphHeight(unsigned bank) {
    switch (bank) {
        case 132: return 5;
        case 133: return 11;
        case 134: return 15;
        case 135: return 19;
        case 136: return 23;
        default: return 11;
    }
}

void DrawText(std::uint8_t* dst, int dst_w, int dst_h, int x, int y,
              std::string_view text, const DrawTextOpts& opts,
              const SpriteBanks& banks, const Palette& pal) {
    if (text.empty()) return;

    auto lut = BuildTintLut(pal, opts.color, opts.brightness);

    auto draw_pass = [&](int ox, int oy, std::uint8_t bri) {
        auto pass_lut = (bri == opts.brightness) ? lut : BuildTintLut(pal, opts.color, bri);
        int cursor = x + ox;
        const int ascii_off = FontAsciiOffset(opts.bank);
        for (char ch : text) {
            unsigned char uc = static_cast<unsigned char>(ch);
            if (uc == 0x20) {
                cursor += opts.width;
                continue;
            }
            if (uc < 0x21 || uc > 0x7F) {
                cursor += opts.width;
                continue;
            }
            int sprite_idx = uc - ascii_off;
            if (sprite_idx >= 0) {
                banks.Blit(dst, dst_w, dst_h, opts.bank, sprite_idx,
                           cursor, y + oy, pass_lut.data(), false);
            }
            cursor += opts.width;
        }
    };

    if (opts.shadow) {
        std::uint8_t sb = opts.brightness > 64 + 8 ? static_cast<std::uint8_t>(opts.brightness - 64) : std::uint8_t{8};
        draw_pass(1, 1, sb);
    }
    draw_pass(0, 0, opts.brightness);
}

void DrawTinyText(std::uint8_t* dst, int dst_w, int dst_h, int x, int y,
                  std::string_view text, std::uint8_t color,
                  std::uint8_t brightness, const SpriteBanks& banks,
                  const Palette& pal) {
    constexpr int kWidth = 4;
    int len = static_cast<int>(text.size());
    int total_w = len * kWidth;
    int start_x = x - total_w / 2;
    DrawTextOpts opts;
    opts.bank = 132;
    opts.width = kWidth;
    opts.color = color;
    opts.brightness = brightness;
    // Need to special-case '1' nudge per docs.
    auto lut = BuildTintLut(pal, color, brightness);
    int cursor = start_x;
    const int ascii_off = FontAsciiOffset(132);
    for (char ch : text) {
        unsigned char uc = static_cast<unsigned char>(ch);
        if (uc == 0x20) { cursor += kWidth; continue; }
        if (uc < 0x21 || uc > 0x7F) { cursor += kWidth; continue; }
        int draw_x = cursor + (ch == '1' ? -1 : 0);
        int sprite_idx = uc - ascii_off;
        if (sprite_idx >= 0) {
            banks.Blit(dst, dst_w, dst_h, 132, sprite_idx, draw_x, y, lut.data(), false);
        }
        cursor += kWidth;
    }
}

}  // namespace silencer
