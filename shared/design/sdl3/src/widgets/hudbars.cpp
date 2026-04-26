#include "hudbars.h"

#include <cstdio>

#include "../font.h"
#include "../sprite.h"

namespace silencer {

void DrawHudBars(std::uint8_t* dst, int dst_w, int dst_h, const HudState& hud,
                 std::uint32_t state_i, const SpriteBanks& banks, const Palette& pal) {
    // The frame and bars are drawn at their sprite-baked offsets; we anchor at
    // the documented HUD positions. The minimap frame is bank 94 idx 0 around
    // (235, 419).
    banks.Blit(dst, dst_w, dst_h, 94, 0, 235, 419);

    // Health (bank 95 idx 0) — the sprite carries its own offset.
    banks.Blit(dst, dst_w, dst_h, 95, 0, 0, 0);
    // Shield (bank 95 idx 1)
    banks.Blit(dst, dst_w, dst_h, 95, 1, 0, 0);
    // Fuel frame + fill
    banks.Blit(dst, dst_w, dst_h, 95, 5, 0, 0);
    banks.Blit(dst, dst_w, dst_h, 95, 6, 0, 0);
    // File progress
    banks.Blit(dst, dst_w, dst_h, 95, 7, 0, 0);

    // Low warnings
    if (hud.health <= 0.5f && (state_i % 8) < 4) {
        banks.Blit(dst, dst_w, dst_h, 95, 3, 0, 0);
    }
    if (hud.shield <= 0.5f && (state_i % 8) < 4) {
        banks.Blit(dst, dst_w, dst_h, 95, 4, 0, 0);
    }

    // Health number (red)
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", hud.health_value);
    DrawTinyText(dst, dst_w, dst_h, 158, 463, buf, 161, 128, banks, pal);

    std::snprintf(buf, sizeof(buf), "%d", hud.shield_value);
    DrawTinyText(dst, dst_w, dst_h, 481, 463, buf, 202, 128, banks, pal);

    // Ammo (bank 135 / width 12)
    std::snprintf(buf, sizeof(buf), "%d", hud.ammo);
    DrawTextOpts ao;
    ao.bank = 135;
    ao.width = 12;
    DrawText(dst, dst_w, dst_h, 117, 457, buf, ao, banks, pal);

    // Credits (bank 135 / width 12, tinted blue)
    std::snprintf(buf, sizeof(buf), "$%d", hud.credits);
    ao.color = 202;
    DrawText(dst, dst_w, dst_h, 572, 456, buf, ao, banks, pal);
}

}  // namespace silencer
