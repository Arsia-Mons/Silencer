#include "scrollbar.h"

#include "../sprite.h"

namespace silencer {

ScrollBar::ScrollBar(int xx, int yy) {
    x = xx; y = yy;
    res_bank = 7;
    res_index = 9;
    draw_visible = true;
}

bool ScrollBar::HitTest(int mx, int my) const {
    return mx >= x && mx < x + 16 && my >= y && my < y + 200;
}

void ScrollBar::OnMouse(const MouseState& m, const DrawCtx& ctx) {
    if (!HitTest(m.x, m.y)) return;
    if (m.clicked) {
        const Sprite* sp = ctx.banks->Get(res_bank, res_index);
        int track_h = sp ? sp->h : 200;
        int local_y = m.y - y;
        if (local_y < 16) ScrollUp();
        else if (local_y > track_h - 16) ScrollDown();
    }
    if (m.wheel > 0) ScrollUp();
    if (m.wheel < 0) ScrollDown();
}

void ScrollBar::Draw(const DrawCtx& ctx) {
    if (!draw_visible) return;
    ctx.banks->Blit(ctx.dst, ctx.dst_w, ctx.dst_h, res_bank, res_index, x, y);
    // Thumb position (rough): scroll_position / scroll_max along the track.
    const Sprite* sp = ctx.banks->Get(res_bank, res_index);
    int track_h = sp ? sp->h : 200;
    int usable = track_h - 32;
    int ty = y + 16 + (scroll_max > 0 ? (scroll_position * usable / scroll_max) : 0);
    ctx.banks->Blit(ctx.dst, ctx.dst_w, ctx.dst_h, res_bank, bar_index, x, ty);
}

}  // namespace silencer
