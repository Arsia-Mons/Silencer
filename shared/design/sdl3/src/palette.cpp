#include "palette.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace silencer {

bool Palette::Load(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "Palette: cannot open %s\n", path.c_str());
        return false;
    }
    // The PALETTE.BIN file is exactly 11 * 768 = 8448 bytes. The §Color System
    // doc mentions a 4-byte header, but the asset on disk is 8448 with no
    // leading header — 11 sub-palettes packed back to back.
    constexpr std::size_t kSubBytes = kColorsPerPalette * 3;  // 768
    constexpr std::size_t kTotal = kSubPaletteCount * kSubBytes;  // 8448

    std::vector<std::uint8_t> buf(kTotal);
    std::size_t got = std::fread(buf.data(), 1, kTotal, f);
    std::fclose(f);
    if (got != kTotal) {
        std::fprintf(stderr, "Palette: short read (%zu / %zu)\n", got, kTotal);
        return false;
    }

    for (std::size_t s = 0; s < kSubPaletteCount; ++s) {
        const std::uint8_t* p = buf.data() + s * kSubBytes;
        for (std::size_t i = 0; i < kColorsPerPalette; ++i) {
            // 6-bit channels (0..63) expanded to 8-bit via `v << 2`.
            palettes_[s][i].r = static_cast<std::uint8_t>(p[i * 3 + 0] << 2);
            palettes_[s][i].g = static_cast<std::uint8_t>(p[i * 3 + 1] << 2);
            palettes_[s][i].b = static_cast<std::uint8_t>(p[i * 3 + 2] << 2);
        }
    }
    return true;
}

void Palette::IndexedToRgba(const std::uint8_t* src, std::uint32_t* dst,
                            std::size_t pixel_count) const {
    const auto& pal = palettes_[0];
    for (std::size_t i = 0; i < pixel_count; ++i) {
        std::uint8_t idx = src[i];
        if (idx == 0) {
            dst[i] = 0x00000000u;  // transparent
            continue;
        }
        const Rgb& c = pal[idx];
        // RGBA8888 little-endian as packed by SDL_PIXELFORMAT_RGBA32.
        dst[i] = (static_cast<std::uint32_t>(0xFFu) << 24) |
                 (static_cast<std::uint32_t>(c.b) << 16) |
                 (static_cast<std::uint32_t>(c.g) << 8) |
                 (static_cast<std::uint32_t>(c.r) << 0);
    }
}

Rgb Palette::ApplyBrightness(Rgb in, std::uint8_t brightness) {
    if (brightness == 128) return in;
    auto lerp = [](int v, float t, int target) {
        float out = v * (1.0f - t) + target * t;
        if (out < 0.0f) out = 0.0f;
        if (out > 255.0f) out = 255.0f;
        return static_cast<std::uint8_t>(out);
    };
    if (brightness > 128) {
        float pct = (brightness - 127) / 128.0f;
        return {lerp(in.r, pct, 255), lerp(in.g, pct, 255), lerp(in.b, pct, 255)};
    }
    float pct = brightness / 128.0f;
    return {static_cast<std::uint8_t>(in.r * pct),
            static_cast<std::uint8_t>(in.g * pct),
            static_cast<std::uint8_t>(in.b * pct)};
}

Rgb Palette::ApplyColorTint(Rgb in, Rgb tint) {
    auto luma = [](Rgb c) {
        return 0.30f * c.r + 0.59f * c.g + 0.11f * c.b;
    };
    float diff = luma(in) - luma(tint);
    auto clamp = [](float v) {
        if (v < 0) return std::uint8_t{0};
        if (v > 255) return std::uint8_t{255};
        return static_cast<std::uint8_t>(v);
    };
    return {clamp(tint.r + diff), clamp(tint.g + diff), clamp(tint.b + diff)};
}

}  // namespace silencer
