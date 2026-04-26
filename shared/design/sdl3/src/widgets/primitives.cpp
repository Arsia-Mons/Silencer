#include "primitives.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace silencer {

void Clear(std::uint8_t* dst, int dst_w, int dst_h, std::uint8_t color) {
    std::memset(dst, color, static_cast<std::size_t>(dst_w) * dst_h);
}

void FilledRect(std::uint8_t* dst, int dst_w, int dst_h, int x1, int y1, int x2, int y2,
                std::uint8_t color) {
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > dst_w) x2 = dst_w;
    if (y2 > dst_h) y2 = dst_h;
    for (int y = y1; y < y2; ++y) {
        std::memset(dst + y * dst_w + x1, color, static_cast<std::size_t>(x2 - x1));
    }
}

void Line(std::uint8_t* dst, int dst_w, int dst_h, int x1, int y1, int x2, int y2,
          std::uint8_t color, int thickness) {
    int dx = std::abs(x2 - x1);
    int dy = -std::abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        FilledRect(dst, dst_w, dst_h, x1, y1, x1 + thickness, y1 + thickness, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void Circle(std::uint8_t* dst, int dst_w, int dst_h, int cx, int cy, int r,
            std::uint8_t color) {
    auto plot = [&](int x, int y) {
        if (x < 0 || x >= dst_w || y < 0 || y >= dst_h) return;
        dst[y * dst_w + x] = color;
    };
    int x = r, y = 0;
    int err = 1 - x;
    while (x >= y) {
        plot(cx + x, cy + y); plot(cx - x, cy + y);
        plot(cx + x, cy - y); plot(cx - x, cy - y);
        plot(cx + y, cy + x); plot(cx - y, cy + x);
        plot(cx + y, cy - x); plot(cx - y, cy - x);
        ++y;
        if (err < 0) err += 2 * y + 1;
        else { --x; err += 2 * (y - x) + 1; }
    }
}

void Checkered(std::uint8_t* dst, int dst_w, int dst_h, int x1, int y1, int x2, int y2,
               std::uint8_t color) {
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > dst_w) x2 = dst_w;
    if (y2 > dst_h) y2 = dst_h;
    for (int y = y1; y < y2; ++y) {
        for (int x = x1 + (y % 2); x < x2; x += 2) {
            dst[y * dst_w + x] = color;
        }
    }
}

}  // namespace silencer
