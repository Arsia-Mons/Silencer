#include "widgets/overlay.h"

#include <array>
#include <cstring>

#include "font.h"
#include "palette.h"
#include "sprite.h"

namespace silencer {

void Overlay::Tick() {
    if (text.empty()) {
        // Sprite-mode animation per bank.
        switch (res_bank) {
            case 208: {
                if (state_i < 60) {
                    res_index = static_cast<uint8_t>(state_i / 2 + 29);
                } else if (state_i < 120) {
                    res_index = 60;
                } else {
                    // fade out then loops; cap at 60
                    int v = static_cast<int>(120 - state_i / 2) + 29;
                    if (v < 29) v = 29;
                    res_index = static_cast<uint8_t>(v);
                }
                if (res_index > 60) res_index = 60;
                break;
            }
            // Other banks not used by main menu; leave res_index alone.
            default:
                break;
        }
    }
    state_i++;
}

void Overlay::Draw(uint8_t *fb, int fb_w, int fb_h,
                   Sprites &sprites, const Palette &palette) const {
    if (!text.empty()) {
        // text mode: draw at raw (x, y), no camera offset.
        DrawText(fb, fb_w, fb_h, x, y, text, sprites,
                 textbank, textwidth, drawalpha, effectcolor,
                 effectbrightness, palette);
        return;
    }
    if (res_bank == 0xFF) return;
    if (!sprites.BankLoaded(res_bank)) return;
    const SpriteBank &b = sprites.Bank(res_bank);
    if (res_index >= b.count) return;
    const Sprite &s = b.sprites[res_index];
    int top_left_x = x - s.offset_x;
    int top_left_y = y - s.offset_y;

    std::array<uint8_t, 256> lut{};
    const uint8_t *lut_ptr = nullptr;
    if (effectbrightness != 128) {
        palette.BuildBrightnessLut(effectbrightness, lut);
        lut_ptr = lut.data();
    }
    Sprites::Blit(fb, fb_w, fb_h, s, top_left_x, top_left_y, lut_ptr);
}

}  // namespace silencer
