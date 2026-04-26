#include "textbox.h"

#include <algorithm>

#include "../font.h"

namespace silencer {

TextBox::TextBox(int xx, int yy, int w, int h) {
    x = xx; y = yy; width = w; height = h;
}

void TextBox::AddLine(const std::string& s, std::uint8_t color, std::uint8_t brightness,
                      bool scroll) {
    if (static_cast<int>(lines.size()) > maxlines) lines.erase(lines.begin());
    int max_chars = width / fontwidth;
    Line ln;
    ln.text = s.substr(0, std::min<std::size_t>(s.size(), static_cast<std::size_t>(max_chars)));
    ln.color = color;
    ln.brightness = brightness;
    lines.push_back(std::move(ln));

    if (scroll) {
        int visible = height / lineheight;
        if (static_cast<int>(lines.size()) > visible) {
            scrolled = static_cast<int>(lines.size()) - visible;
        } else {
            scrolled = 0;
        }
    }
}

void TextBox::Draw(const DrawCtx& ctx) {
    if (!draw_visible) return;
    int visible = height / lineheight;
    int end = std::min(scrolled + visible, static_cast<int>(lines.size()));
    for (int i = scrolled; i < end; ++i) {
        const Line& ln = lines[i];
        DrawTextOpts opts;
        opts.bank = res_text_bank;
        opts.width = fontwidth;
        opts.color = ln.color;
        opts.brightness = ln.brightness;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, x, y + (i - scrolled) * lineheight, ln.text,
                 opts, *ctx.banks, *ctx.palette);
    }
}

}  // namespace silencer
