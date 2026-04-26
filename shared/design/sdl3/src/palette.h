#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace silencer {

struct Rgb {
    uint8_t r, g, b;
};

class Palette {
public:
    // 11 sub-palettes, each 256 entries
    static constexpr int kSubPalettes = 11;
    static constexpr int kColors = 256;

    bool Load(const std::string &palette_bin_path);

    // 0..10
    void SetActive(int sub) { active_ = sub; }
    int Active() const { return active_; }

    const Rgb &Color(int sub, int idx) const { return tables_[sub][idx]; }
    const Rgb &Active(int idx) const { return tables_[active_][idx]; }

    // Build a 256-byte brightness LUT against the active sub-palette.
    // For each input index i, find the entry j whose RGB best matches
    // i's RGB scaled by (brightness/128.0). Returns the LUT as 256 bytes.
    void BuildBrightnessLut(int brightness, std::array<uint8_t, 256> &out) const;

private:
    std::array<std::array<Rgb, kColors>, kSubPalettes> tables_{};
    int active_ = 0;
};

}  // namespace silencer
