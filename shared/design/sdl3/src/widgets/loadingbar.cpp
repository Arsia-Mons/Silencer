#include "loadingbar.h"

#include "primitives.h"

namespace silencer {

void DrawLoadingBar(std::uint8_t* dst, int dst_w, int dst_h, float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    int total = 500;
    int filled = static_cast<int>(progress * total);
    int x1 = (640 - total) / 2;     // 70
    int y1 = (480 - 20) / 2;        // 230
    int x2 = (640 + filled) / 2;
    int y2 = (480 + 20) / 2;        // 250
    FilledRect(dst, dst_w, dst_h, x1, y1, x2, y2, 123);
}

}  // namespace silencer
