#include "selectbox.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "../font.h"
#include "primitives.h"

namespace silencer {

SelectBox::SelectBox(int xx, int yy, int w, int h, int lh)
    : width(w), height(h), lineheight(lh) {
    x = xx; y = yy;
}

void SelectBox::AddItem(const std::string& s, int id) {
    items.push_back(s);
    item_ids.push_back(id);
    int visible = height / lineheight;
    if (static_cast<int>(items.size()) > visible) {
        scrolled = static_cast<int>(items.size()) - visible;
    } else {
        scrolled = 0;
    }
}

bool SelectBox::HitTest(int mx, int my) const {
    int x2 = x + width - 16;  // reserve scrollbar
    return mx > x && mx < x2 && my > y && my < y + height;
}

void SelectBox::OnMouse(const MouseState& m, const DrawCtx&) {
    if (HitTest(m.x, m.y) && m.clicked) {
        int idx = (m.y - y) / lineheight + scrolled;
        if (idx >= 0 && idx < static_cast<int>(items.size())) selected_item = idx;
    }
}

void SelectBox::OnKey(int kc) {
    if (kc == SDLK_DOWN) {
        if (selected_item < static_cast<int>(items.size()) - 1) selected_item++;
    } else if (kc == SDLK_UP) {
        if (selected_item > 0) selected_item--;
    } else if (kc == SDLK_RETURN) {
        enter_pressed = true;
    }
}

void SelectBox::Draw(const DrawCtx& ctx) {
    if (!draw_visible) return;
    int visible = height / lineheight;
    int end = std::min(scrolled + visible, static_cast<int>(items.size()));
    for (int i = scrolled; i < end; ++i) {
        int line = i - scrolled;
        int yy = y + line * lineheight;
        if (i == selected_item) {
            FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, x, yy, x + width, yy + 11, 180);
        }
        DrawTextOpts opts;
        opts.bank = 133;
        opts.width = 6;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, x, yy, items[i], opts, *ctx.banks, *ctx.palette);
    }
}

}  // namespace silencer
