// Loading bar — palette index 123 fill, 500x20 centered at (320, 240).
#pragma once

#include <cstdint>

namespace silencer {

void DrawLoadingBar(std::uint8_t* dst, int dst_w, int dst_h, float progress);

}  // namespace silencer
