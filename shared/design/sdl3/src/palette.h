// Palette loader & color transforms — see docs/design/palette.md.
//
// The 11 sub-palettes in PALETTE.BIN drive different game states; e.g. the
// main menu uses sub-palette 1. Set the active sub-palette via SetActive()
// before any IndexedToRgba conversion or `Color(index)` lookup.
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

class Palette {
   public:
    static constexpr std::size_t kSubPaletteCount = 11;
    static constexpr std::size_t kColorsPerPalette = 256;

    bool Load(const std::string& palette_bin_path);

    // Set the active sub-palette. Used by IndexedToRgba and the (sub-less)
    // overload of Color(index). Defaults to 0 (in-game default per spec).
    void SetActive(std::size_t sub) { active_ = sub < kSubPaletteCount ? sub : 0; }
    std::size_t Active() const { return active_; }

    const Rgb& Color(std::size_t sub, std::size_t index) const {
        return palettes_[sub][index];
    }
    const Rgb& Color(std::size_t index) const { return palettes_[active_][index]; }

    // Convert an 8-bit indexed buffer to RGBA8888 using the active sub-palette.
    // Index 0 is treated as transparent (alpha 0).
    void IndexedToRgba(const std::uint8_t* src, std::uint32_t* dst, std::size_t pixel_count) const;

    // EffectBrightness — see docs/design/palette.md.
    static Rgb ApplyBrightness(Rgb in, std::uint8_t brightness);

    // EffectColor — luminance-preserving tint. Not used by the main menu;
    // kept here so the API is stable when more screens are added.
    static Rgb ApplyColorTint(Rgb in, Rgb tint);

   private:
    using SubPalette = std::array<Rgb, kColorsPerPalette>;
    std::array<SubPalette, kSubPaletteCount> palettes_{};
    std::size_t active_{0};
};

}  // namespace silencer
