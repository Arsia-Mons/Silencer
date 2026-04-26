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
    std::fseek(f, 0, SEEK_END);
    long file_bytes = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(file_bytes));
    std::size_t got = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    if (got != buf.size()) {
        std::fprintf(stderr, "Palette: short read (%zu / %zu)\n", got, buf.size());
        return false;
    }

    // Layout per clients/silencer/src/palette.cpp:43..54 — sub-palette `s`
    // begins at `4 + s * (768 + 4)`. The first 4 bytes of the file (and the
    // 4 between each sub-palette) are skipped. NB: the on-disk file is 8448
    // bytes, smaller than `4 + 11*(768+4) = 8496`, so the last sub-palette
    // reads fall partially past EOF in the real client too — we simply leave
    // those tail bytes as zero, matching what the engine ends up with.
    constexpr std::size_t kSubBytes = kColorsPerPalette * 3;  // 768
    for (std::size_t s = 0; s < kSubPaletteCount; ++s) {
        std::size_t off = 4 + s * (kSubBytes + 4);
        for (std::size_t i = 0; i < kColorsPerPalette; ++i) {
            std::size_t base = off + i * 3;
            std::uint8_t r = (base + 0 < buf.size()) ? buf[base + 0] : 0;
            std::uint8_t g = (base + 1 < buf.size()) ? buf[base + 1] : 0;
            std::uint8_t b = (base + 2 < buf.size()) ? buf[base + 2] : 0;
            // 6-bit channels (0..63) expanded to 8-bit via `v << 2`.
            palettes_[s][i].r = static_cast<std::uint8_t>(r << 2);
            palettes_[s][i].g = static_cast<std::uint8_t>(g << 2);
            palettes_[s][i].b = static_cast<std::uint8_t>(b << 2);
        }
    }
    return true;
}

void Palette::IndexedToRgba(const std::uint8_t* src, std::uint32_t* dst,
                            std::size_t pixel_count) const {
    const auto& pal = palettes_[active_];
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
