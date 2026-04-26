#include "panel.h"

#include <algorithm>

#include "../sprite.h"

namespace silencer {

void DrawHStretchPanel(std::uint8_t* dst, int dst_w, int dst_h, int x, int y, int w, int h,
                       const SpriteBanks& banks) {
    constexpr unsigned kBank = 188;
    auto blit = [&](unsigned idx, int xx, int yy) {
        banks.Blit(dst, dst_w, dst_h, kBank, idx, xx, yy);
    };
    auto sw = [&](unsigned idx) {
        const Sprite* s = banks.Get(kBank, idx);
        return s ? s->w : 16;
    };

    // Top row.
    blit(0, x, y);
    int tile_x = sw(0);
    while (tile_x < w - sw(2)) {
        int clip_w = std::min(w - tile_x - 36, static_cast<int>(sw(1)));
        if (clip_w <= 0) break;
        // We don't have a srcW-clipping blit — full-width is acceptable for demo.
        blit(1, x + tile_x, y);
        tile_x += clip_w;
    }
    blit(2, x + w - 36, y);

    // Bottom row.
    blit(6, x, y + h);
    tile_x = sw(6);
    while (tile_x < w - sw(8)) {
        int clip_w = std::min(w - tile_x - 36, static_cast<int>(sw(7)));
        if (clip_w <= 0) break;
        blit(7, x + tile_x, y + h);
        tile_x += clip_w;
    }
    blit(8, x + w - 36, y + h);
}

}  // namespace silencer
