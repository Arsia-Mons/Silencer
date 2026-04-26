#include "textinput.h"

#include <SDL3/SDL.h>

#include <algorithm>

#include "../font.h"
#include "../palette.h"
#include "../sprite.h"
#include "primitives.h"

namespace silencer {

TextInput::TextInput(int xx, int yy, int w, int h, unsigned bank, int fw, int mc, int mw)
    : width(w), height(h), res_text_bank(bank), fontwidth(fw), maxchars(mc), maxwidth(mw) {
    x = xx;
    y = yy;
}

bool TextInput::HitTest(int mx, int my) const {
    return mx > x && mx < x + width && my > y && my < y + height;
}

void TextInput::OnMouse(const MouseState& m, const DrawCtx&) {
    if (HitTest(m.x, m.y) && m.clicked) {
        show_caret = true;
        focused = true;
    }
}

void TextInput::OnKey(int kc) {
    if (inactive) return;
    if (kc == SDLK_BACKSPACE) {
        if (!text.empty()) {
            text.pop_back();
            if (scrolled > 0) scrolled--;
        }
    }
}

void TextInput::OnTextInput(const char* utf8) {
    if (inactive) return;
    for (const char* p = utf8; *p; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x20 || c > 0x7F) continue;
        if (numbers_only && (c < '0' || c > '9')) continue;
        if (static_cast<int>(text.size()) >= maxchars) return;
        if (static_cast<int>(text.size()) >= maxwidth + scrolled) scrolled++;
        text.push_back(static_cast<char>(c));
    }
}

void TextInput::Draw(const DrawCtx& ctx) {
    if (!draw_visible) return;

    std::string view;
    if (scrolled < static_cast<int>(text.size())) view = text.substr(scrolled);
    if (password) view.assign(view.size(), '*');

    std::uint8_t bri = inactive ? std::uint8_t{64} : effect_brightness;
    DrawTextOpts opts;
    opts.bank = res_text_bank;
    opts.width = fontwidth;
    opts.color = effect_color;
    opts.brightness = bri;
    DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, x, y, view, opts, *ctx.banks, *ctx.palette);

    if (!inactive && show_caret && (ctx.state_i % 32 < 16)) {
        int cx = x + static_cast<int>(view.size()) * fontwidth;
        int cy = y - 1;
        int cw = 1;
        int ch = static_cast<int>(height * 0.8);
        FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, cx, cy, cx + cw, cy + ch, caret_color);
    }
}

}  // namespace silencer
