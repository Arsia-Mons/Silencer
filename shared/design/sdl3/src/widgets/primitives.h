// Drawing primitives — operate on the 8-bit indexed framebuffer.
#pragma once

#include <cstdint>

namespace silencer {

void FilledRect(std::uint8_t* dst, int dst_w, int dst_h,
                int x1, int y1, int x2, int y2, std::uint8_t color);

void Line(std::uint8_t* dst, int dst_w, int dst_h,
          int x1, int y1, int x2, int y2, std::uint8_t color, int thickness = 1);

void Circle(std::uint8_t* dst, int dst_w, int dst_h,
            int cx, int cy, int radius, std::uint8_t color);

void Checkered(std::uint8_t* dst, int dst_w, int dst_h,
               int x1, int y1, int x2, int y2, std::uint8_t color);

void Clear(std::uint8_t* dst, int dst_w, int dst_h, std::uint8_t color);

}  // namespace silencer
