#include "minimap.h"

#include <cstdlib>
#include <cstring>

#include "../sprite.h"

namespace silencer {

Minimap::Minimap() { GenerateDemoWorld(); }

void Minimap::Plot(int x, int y, std::uint8_t color) {
    if (x < 0 || x >= kW || y < 0 || y >= kH) return;
    pixels[y * kW + x] = color;
}

void Minimap::Marker(int x, int y, std::uint8_t color) {
    Plot(x, y, color);
    Plot(x - 1, y, color);
    Plot(x + 1, y, color);
    Plot(x, y - 1, color);
    Plot(x, y + 1, color);
}

void Minimap::GenerateDemoWorld() {
    // Fill with a dark gray and sprinkle "rooms" + corridors.
    pixels.fill(5);  // gray ramp
    // Border
    for (int x = 0; x < kW; ++x) { Plot(x, 0, 17); Plot(x, kH - 1, 17); }
    for (int y = 0; y < kH; ++y) { Plot(0, y, 17); Plot(kW - 1, y, 17); }
    // Rooms (filled rectangles)
    auto rect = [&](int x1, int y1, int x2, int y2, std::uint8_t c) {
        for (int yy = y1; yy <= y2; ++yy)
            for (int xx = x1; xx <= x2; ++xx) Plot(xx, yy, c);
    };
    rect(10, 10, 50, 25, 14);
    rect(70, 10, 100, 30, 14);
    rect(115, 8, 160, 22, 14);
    rect(20, 35, 90, 55, 14);
    rect(110, 35, 158, 55, 14);
    // Markers — players, beacons.
    Marker(30, 18, 161);  // red
    Marker(80, 22, 97);   // blue
    Marker(135, 14, 113); // green
    Marker(135, 45, 224); // beacon green
}

void Minimap::Draw(std::uint8_t* dst, int dst_w, int dst_h, const SpriteBanks& banks) const {
    // Frame
    banks.Blit(dst, dst_w, dst_h, 94, 0, 235, 419);

    // Blit pixel buffer
    int ox = 235;
    int oy = 419;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            std::uint8_t p = pixels[y * kW + x];
            if (p == 0) continue;
            int dx = ox + x;
            int dy = oy + y;
            if (dx >= 0 && dx < dst_w && dy >= 0 && dy < dst_h) {
                dst[dy * dst_w + dx] = p;
            }
        }
    }
}

}  // namespace silencer
