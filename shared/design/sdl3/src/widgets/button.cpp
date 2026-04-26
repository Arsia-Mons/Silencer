#include "button.h"

#include <array>

#include "../font.h"
#include "../palette.h"
#include "../sprite.h"

namespace silencer {

namespace {
const ButtonVariant kVariants[] = {
    /* B112x33   */ {112, 33,  6, 28, 135, 11, 8, 0, false, false},
    /* B196x33   */ {196, 33,  6,  7, 135, 11, 8, 0, false, false},
    /* B220x33   */ {220, 33,  6, 23, 135, 11, 8, 0, false, false},
    /* B236x27   */ {236, 27,  6,  2, 135, 11, 8, 0, false, false},
    /* B52x21    */ { 52, 21,  0,  0, 133,  7, 8, 1, false, false},
    /* B156x21   */ {156, 21,  7, 24, 134,  8, 4, 0,  true, false},
    /* BCheckbox */ { 13, 13,  7, 19,   0,  0, 0, 0, false,  true},
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
    if (t == ButtonType::BCheckbox) {
        res_index = checked ? 18 : 19;
    }
}

void Button::Tick() {
    const auto& v = GetButtonVariant(type);
    if (v.no_anim) return;

    switch (state_) {
        case kActivating:
            state_i_++;
            effect_brightness = static_cast<std::uint8_t>(128 + state_i_ * 2);
            if (!v.brightness_only) {
                res_index = static_cast<std::uint8_t>(v.base_index + state_i_);
            }
            if (state_i_ >= 4) {
                state_ = kActive;
                state_i_ = 0;
                effect_brightness = 136;
                if (!v.brightness_only) {
                    res_index = static_cast<std::uint8_t>(v.base_index + 4);
                }
            }
            break;
        case kActive:
            effect_brightness = 136;
            if (!v.brightness_only) {
                res_index = static_cast<std::uint8_t>(v.base_index + 4);
            }
            break;
        case kDeactivating:
            state_i_++;
            effect_brightness = static_cast<std::uint8_t>(128 + (4 - state_i_) * 2);
            if (!v.brightness_only) {
                res_index = static_cast<std::uint8_t>(v.base_index + (4 - state_i_));
            }
            if (state_i_ >= 4) {
                state_ = kInactive;
                state_i_ = 0;
                effect_brightness = 128;
                if (!v.brightness_only) res_index = static_cast<std::uint8_t>(v.base_index);
            }
            break;
        case kInactive:
            effect_brightness = 128;
            if (!v.brightness_only) res_index = static_cast<std::uint8_t>(v.base_index);
            break;
    }
}

bool Button::HitTest(int mx, int my) const {
    if (!draw_visible) return false;
    const auto& v = GetButtonVariant(type);
    int x1 = x;
    int y1 = y;
    if (v.sprite_bank != 0) {
        // For sprite-backed buttons, the docs use (x - offsetX) but our demo
        // places (x, y) at the sprite's anchor — so the bounds match the
        // documented formula: hit-rect = (x - offsetX, y - offsetY, +width, +height).
        // We don't have the sprite here without a banks ref — hot-rect is
        // approximated as (x, y, x+w, y+h) since callers position the anchor.
    }
    int x2 = x1 + v.width;
    int y2 = y1 + v.height;
    return mx > x1 && mx < x2 && my > y1 && my < y2;
}

void Button::OnMouse(const MouseState& m, const DrawCtx& ctx) {
    if (!draw_visible) return;
    bool inside = HitTest(m.x, m.y);
    if (inside && (state_ == kInactive || state_ == kDeactivating)) {
        state_ = kActivating;
        state_i_ = 0;
    } else if (!inside && (state_ == kActive || state_ == kActivating)) {
        state_ = kDeactivating;
        state_i_ = 0;
    }
    if (inside && m.clicked) {
        clicked = true;
        if (type == ButtonType::BCheckbox) {
            checked = !checked;
            res_index = checked ? 18 : 19;
        }
    }
}

void Button::Draw(const DrawCtx& ctx) {
    if (!draw_visible) return;
    const auto& v = GetButtonVariant(type);

    // Build a per-button tint LUT so brightness changes recolor sprite pixels.
    // For demo simplicity we apply EffectBrightness only (color stays 0).
    std::array<std::uint8_t, 256> lut{};
    for (int i = 0; i < 256; ++i) lut[i] = static_cast<std::uint8_t>(i);
    if (v.sprite_bank != 0) {
        // Use font's nearest-neighbor LUT computation by inlining a small version.
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
        const Sprite* sp = ctx.banks->Get(res_bank, res_index);
        if (sp) {
            // The sprite's offset_x/offset_y already shift the anchor — the
            // docs say `blit at (x - offsetX, y - offsetY)`. Our Blit() does
            // that internally given (x, y) as the anchor.
            ctx.banks->Blit(ctx.dst, ctx.dst_w, ctx.dst_h, res_bank, res_index, x, y, lut.data());
        }
    }

    if (v.text_bank != 0 && !text.empty()) {
        // Center text: xoff = (width - len*advance)/2 (+1 for B52x21)
        int len = static_cast<int>(text.size());
        int xoff = (v.width - len * v.text_advance) / 2 + v.text_xoff_extra;
        int tx = x + xoff;
        int ty = y + v.text_yoff;
        DrawTextOpts opts;
        opts.bank = v.text_bank;
        opts.width = v.text_advance;
        opts.brightness = effect_brightness;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, tx, ty, text, opts, *ctx.banks, *ctx.palette);
    }
}

}  // namespace silencer
