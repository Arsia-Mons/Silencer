#include "toggle.h"

#include <array>

#include "../font.h"
#include "../palette.h"
#include "../sprite.h"

namespace silencer {

Toggle::Toggle(ToggleMode m, int xx, int yy, std::uint8_t sid, std::uint8_t agency,
               std::string lab)
    : set_id(sid), agency_index(agency), label(std::move(lab)), mode(m) {
    x = xx;
    y = yy;
    if (mode == ToggleMode::Agency) {
        res_bank = 181;
        res_index = agency_index;
        effect_color = 112;
        effect_brightness = selected ? 128 : 32;
    } else {
        res_bank = 7;
        res_index = selected ? 18 : 19;
    }
}

bool Toggle::HitTest(int mx, int my) const {
    // Approx bounds since we don't have sprite dims here.
    int w = (mode == ToggleMode::Agency) ? 32 : 13;
    int h = (mode == ToggleMode::Agency) ? 32 : 13;
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

void Toggle::OnMouse(const MouseState& m, const DrawCtx&) {
    if (HitTest(m.x, m.y) && m.clicked) {
        selected = !selected;
        if (mode == ToggleMode::Agency) {
            effect_brightness = selected ? 128 : 32;
        } else {
            res_index = selected ? 18 : 19;
        }
    }
}

void Toggle::Draw(const DrawCtx& ctx) {
    if (!draw_visible) return;

    if (mode == ToggleMode::Agency) {
        effect_brightness = selected ? 128 : 32;
        res_index = agency_index;
    } else {
        res_index = selected ? 18 : 19;
    }

    // Build LUT for current effect state.
    std::array<std::uint8_t, 256> lut{};
    for (int i = 0; i < 256; ++i) lut[i] = static_cast<std::uint8_t>(i);
    bool has_color = (effect_color != 0);
    bool has_bri = (effect_brightness != 128);
    if (has_color || has_bri) {
        Rgb tint = ctx.palette->Color(effect_color);
        for (int i = 2; i < 256; ++i) {
            Rgb c = ctx.palette->Color(i);
            if (has_color) c = Palette::ApplyColorTint(c, tint);
            if (has_bri) c = Palette::ApplyBrightness(c, effect_brightness);
            int best = i;
            long bd = (1L << 30);
            for (int j = 2; j < 256; ++j) {
                const Rgb& p = ctx.palette->Color(j);
                long dr = int(c.r) - int(p.r);
                long dg = int(c.g) - int(p.g);
                long db = int(c.b) - int(p.b);
                long d = dr * dr + dg * dg + db * db;
                if (d < bd) { bd = d; best = j; if (d == 0) break; }
            }
            lut[i] = static_cast<std::uint8_t>(best);
        }
    }
    ctx.banks->Blit(ctx.dst, ctx.dst_w, ctx.dst_h, res_bank, res_index, x, y, lut.data());

    if (!label.empty()) {
        // Center label horizontally on (x), font 134 / width 9, y on toggle's y.
        int len = static_cast<int>(label.size());
        int lx = x - (len * 9) / 2;
        DrawTextOpts opts;
        opts.bank = 134;
        opts.width = 9;
        opts.brightness = 128;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, lx, y, label, opts, *ctx.banks, *ctx.palette);
    }
}

}  // namespace silencer
