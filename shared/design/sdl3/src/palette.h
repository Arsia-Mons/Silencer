// Palette loader & color transforms (per docs/design-system.md §Color System).
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace silencer {

struct Rgb {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
};

// 11 sub-palettes of 256 RGB triples (per spec). Sub-palette 0 is the UI default.
class Palette {
   public:
    static constexpr std::size_t kSubPaletteCount = 11;
    static constexpr std::size_t kColorsPerPalette = 256;

    bool Load(const std::string& palette_bin_path);

    const Rgb& Color(std::size_t sub, std::size_t index) const {
        return palettes_[sub][index];
    }
    const Rgb& Color(std::size_t index) const { return palettes_[0][index]; }

    // Convert a 640x480 8-bit indexed buffer to RGBA8888 using sub-palette 0.
    // Index 0 is treated as transparent (alpha 0).
    void IndexedToRgba(const std::uint8_t* src, std::uint32_t* dst, std::size_t pixel_count) const;

    // EffectBrightness — see §Color System / Brightness Transform.
    // Returns the *RGB color* after a brightness transform; index lookup
    // (nearest-neighbor in palette) is left to callers when needed.
    static Rgb ApplyBrightness(Rgb in, std::uint8_t brightness);

    // EffectColor — luminance-preserving tint.
    static Rgb ApplyColorTint(Rgb in, Rgb tint);

   private:
    using SubPalette = std::array<Rgb, kColorsPerPalette>;
    std::array<SubPalette, kSubPaletteCount> palettes_{};
};

}  // namespace silencer
