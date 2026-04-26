// Minimap — 172x62 paletted buffer at (235, 419), framed by sprite bank 94 idx 0.
#pragma once

#include <array>
#include <cstdint>

namespace silencer {

class SpriteBanks;

class Minimap {
   public:
    static constexpr int kW = 172;
    static constexpr int kH = 62;

    Minimap();

    // Set a single pixel in the minimap buffer.
    void Plot(int x, int y, std::uint8_t color);

    // Draw a small "+" marker at (x, y).
    void Marker(int x, int y, std::uint8_t color);

    // Generate a sample world background (called once on init).
    void GenerateDemoWorld();

    void Draw(std::uint8_t* dst, int dst_w, int dst_h, const SpriteBanks& banks) const;

    std::array<std::uint8_t, kW * kH> pixels{};
};

}  // namespace silencer
