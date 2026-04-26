#include "overlay.h"

#include <cstdlib>

#include "../font.h"
#include "../sprite.h"

namespace silencer {

void Overlay::Tick() {
    state_i_anim++;
    // Apply per-bank animation rules from §Overlay.
    switch (res_bank) {
        case 54:
            res_index = static_cast<std::uint8_t>(state_i_anim % 10);
            break;
        case 56:
            res_index = 0;
            break;
        case 57:
        case 58: {
            std::uint32_t t = state_i_anim;
            std::uint32_t f = t / 4;
            if (f >= 16 && (std::rand() % 100 == 0)) state_i_anim = 0;
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
