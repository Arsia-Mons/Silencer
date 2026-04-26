#include "button.h"

#include <array>

#include "../font.h"
#include "../palette.h"
#include "../sprite.h"

namespace silencer {

namespace {
// Currently scoped to B196x33 only — see docs/design/widget-button.md.
const ButtonVariant kVariants[] = {
    /* B196x33 */ {196, 33, 6, 7, 135, 11, 8},
};
}

const ButtonVariant& GetButtonVariant(ButtonType t) { return kVariants[static_cast<int>(t)]; }

Button::Button(ButtonType t, int xx, int yy, std::string txt)
    : text(std::move(txt)), type(t) {
    x = xx;
    y = yy;
    const auto& v = GetButtonVariant(t);
    res_bank = static_cast<std::uint8_t>(v.sprite_bank);
    res_index = static_cast<std::uint8_t>(v.base_index);
}

void Button::Tick() {
    const auto& v = GetButtonVariant(type);
    switch (state_) {
        case kActivating:
            state_i_++;
            effect_brightness = static_cast<std::uint8_t>(128 + state_i_ * 2);
            res_index = static_cast<std::uint8_t>(v.base_index + state_i_);
            if (state_i_ >= 4) {
                state_ = kActive;
                state_i_ = 0;
                effect_brightness = 136;
                res_index = static_cast<std::uint8_t>(v.base_index + 4);
            }
            break;
        case kActive:
            effect_brightness = 136;
            res_index = static_cast<std::uint8_t>(v.base_index + 4);
            break;
        case kDeactivating:
            state_i_++;
            effect_brightness = static_cast<std::uint8_t>(128 + (4 - state_i_) * 2);
            res_index = static_cast<std::uint8_t>(v.base_index + (4 - state_i_));
            if (state_i_ >= 4) {
                state_ = kInactive;
                state_i_ = 0;
                effect_brightness = 128;
                res_index = static_cast<std::uint8_t>(v.base_index);
            }
            break;
        case kInactive:
            effect_brightness = 128;
            res_index = static_cast<std::uint8_t>(v.base_index);
            break;
    }
}

bool Button::HitTest(int mx, int my) const {
    if (!draw_visible) return false;
    const auto& v = GetButtonVariant(type);
    // Hit-rect = anchor-shifted sprite footprint.
    // top-left = (x - sprite.offset_x, y - sprite.offset_y).
    // We don't have a banks pointer here; the caller owns spatial routing
    // via the Interface, which calls OnMouse with the live banks/palette.
    int x1 = x;
    int y1 = y;
    int x2 = x1 + v.width;
    int y2 = y1 + v.height;
    return mx > x1 && mx < x2 && my > y1 && my < y2;
}

void Button::OnMouse(const MouseState& m, const DrawCtx& ctx) {
    if (!draw_visible) return;
    // Real hit test honors the sprite anchor. Compute it inline using ctx.
    const auto& v = GetButtonVariant(type);
    const Sprite* sp = ctx.banks->Get(res_bank, res_index);
    int x1 = x - (sp ? sp->offset_x : 0);
    int y1 = y - (sp ? sp->offset_y : 0);
    int x2 = x1 + v.width;
    int y2 = y1 + v.height;
    bool inside = (m.x > x1 && m.x < x2 && m.y > y1 && m.y < y2);

    if (inside && (state_ == kInactive || state_ == kDeactivating)) {
        state_ = kActivating;
        state_i_ = 0;
    } else if (!inside && (state_ == kActive || state_ == kActivating)) {
        state_ = kDeactivating;
        state_i_ = 0;
    }
    if (inside && m.clicked) clicked = true;
}

void Button::Draw(const DrawCtx& ctx) {
    if (!draw_visible) return;
    const auto& v = GetButtonVariant(type);

    // Build a brightness-only LUT so the chrome ramps 128→136 during hover.
    // EffectBrightness mixes the source RGB toward white (>128) or black (<128)
    // by `(b - 128) / 128`, then snaps each result back to a palette index by
    // nearest-RGB match. Identity LUT is fast-pathed when brightness is neutral.
    std::array<std::uint8_t, 256> lut{};
    for (int i = 0; i < 256; ++i) lut[i] = static_cast<std::uint8_t>(i);
    if (effect_brightness != 128) {
        for (int i = 2; i < 256; ++i) {
            Rgb c = Palette::ApplyBrightness(ctx.palette->Color(i), effect_brightness);
            int best = i;
            long best_d = (1L << 30);
            for (int j = 2; j < 256; ++j) {
                const Rgb& p = ctx.palette->Color(j);
                long dr = int(c.r) - int(p.r);
                long dg = int(c.g) - int(p.g);
                long db = int(c.b) - int(p.b);
                long d = dr * dr + dg * dg + db * db;
                if (d < best_d) { best_d = d; best = j; if (d == 0) break; }
            }
            lut[i] = static_cast<std::uint8_t>(best);
        }
    }

    // Chrome.
    const Sprite* sp = ctx.banks->Get(res_bank, res_index);
    if (sp) {
        ctx.banks->Blit(ctx.dst, ctx.dst_w, ctx.dst_h, res_bank, res_index, x, y, lut.data());
    }

    // Label, centered against the rendered (anchor-shifted) sprite top-left.
    if (v.text_bank != 0 && !text.empty() && sp) {
        int len = static_cast<int>(text.size());
        int xoff = (v.width - len * v.text_advance) / 2;
        int tx = x - sp->offset_x + xoff;
        int ty = y - sp->offset_y + v.text_yoff;
        DrawTextOpts opts;
        opts.bank = v.text_bank;
        opts.width = v.text_advance;
        opts.brightness = effect_brightness;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, tx, ty, text, opts, *ctx.banks, *ctx.palette);
    }
}

}  // namespace silencer
