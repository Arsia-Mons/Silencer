#include "overlay.h"

#include <cstdlib>

#include "../font.h"
#include "../sprite.h"

namespace silencer {

void Overlay::Tick() {
    // Bump first, then resolve per-bank animation rules
    // (matches clients/silencer/src/overlay.cpp:18..79).
    switch (res_bank) {
        case 208: {
            // Main-menu logo: fade in 29→60 over 60 ticks, hold for 60 ticks,
            // fade back out, loop. See docs/design/widget-overlay.md.
            std::uint32_t s = state_i_anim;
            int idx;
            if (s < 60u) {
                idx = static_cast<int>(s / 2) + 29;
            } else if (s < 120u) {
                idx = 60;
            } else {
                idx = (120 - static_cast<int>(s / 2)) + 29;
                if (idx <= 29) {
                    state_i_anim = static_cast<std::uint32_t>(-1);  // ++ below loops to 0
                    idx = 29;
                }
            }
            if (idx > 60) idx = 60;
            res_index = static_cast<std::uint8_t>(idx);
            break;
        }
        case 54:
            res_index = static_cast<std::uint8_t>(state_i_anim % 10);
            break;
        case 56:
            res_index = 0;
            break;
        case 57:
        case 58: {
            std::uint32_t f = state_i_anim / 4;
            if (f >= 16 && (std::rand() % 100 == 0)) state_i_anim = static_cast<std::uint32_t>(-1);
            res_index = static_cast<std::uint8_t>(f > 16 ? 16 : f);
            break;
        }
        case 171:
            res_index = static_cast<std::uint8_t>((state_i_anim / 2) % 4);
            break;
        case 222:
            res_index = static_cast<std::uint8_t>(state_i_anim);
            if (state_i_anim >= 3) destroyed = true;
            break;
        default: break;
    }
    state_i_anim++;
}

bool Overlay::HitTest(int mx, int my) const {
    if (!text.empty()) {
        int x1 = x;
        int x2 = x + static_cast<int>(text.size()) * text_width;
        int y1 = y;
        int y2 = y + FontGlyphHeight(text_bank);
        return mx >= x1 && mx < x2 && my >= y1 && my < y2;
    }
    // Sprite mode bounds (approx, no sprite ref here).
    return false;
}

void Overlay::OnMouse(const MouseState& m, const DrawCtx&) {
    if (HitTest(m.x, m.y) && m.clicked) clicked = true;
}

void Overlay::Draw(const DrawCtx& ctx) {
    if (!draw_visible || destroyed) return;
    if (!text.empty()) {
        DrawTextOpts opts;
        opts.bank = text_bank;
        opts.width = text_width;
        opts.color = effect_color;
        opts.brightness = effect_brightness;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, x, y, text, opts, *ctx.banks, *ctx.palette);
    } else {
        ctx.banks->Blit(ctx.dst, ctx.dst_w, ctx.dst_h, res_bank, res_index, x, y, nullptr,
                        mirrored);
    }
}

}  // namespace silencer
