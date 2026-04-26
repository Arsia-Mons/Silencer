#include "widgets/button.h"

#include <array>

#include "font.h"
#include "palette.h"
#include "sprite.h"

namespace silencer {

void Button::SetHovered(bool hovered) {
    if (hovered) {
        if (state == ButtonState::Inactive ||
            state == ButtonState::Deactivating) {
            state = ButtonState::Activating;
            state_i = 0;
        }
    } else {
        if (state == ButtonState::Active ||
            state == ButtonState::Activating) {
            state = ButtonState::Deactivating;
            state_i = 0;
        }
    }
}

void Button::Tick() {
    switch (state) {
        case ButtonState::Inactive:
            res_index = base_index;        // 7
            effectbrightness = 128;
            break;
        case ButtonState::Activating: {
            int si = static_cast<int>(state_i);
            if (si > 4) si = 4;
            res_index = static_cast<uint8_t>(base_index + si);
            effectbrightness = static_cast<uint8_t>(128 + si * 2);
            if (state_i >= 4) {
                state = ButtonState::Active;
                state_i = 0;
                res_index = static_cast<uint8_t>(base_index + 4);  // 11
                effectbrightness = 136;
            }
            break;
        }
        case ButtonState::Active:
            res_index = static_cast<uint8_t>(base_index + 4);  // 11
            effectbrightness = 136;
            break;
        case ButtonState::Deactivating: {
            int si = static_cast<int>(state_i);
            if (si > 4) si = 4;
            int rev = 4 - si;
            res_index = static_cast<uint8_t>(base_index + rev);
            effectbrightness = static_cast<uint8_t>(128 + rev * 2);
            if (state_i >= 4) {
                state = ButtonState::Inactive;
                state_i = 0;
                res_index = base_index;
                effectbrightness = 128;
            }
            break;
        }
    }
    state_i++;
}

void Button::Draw(uint8_t *fb, int fb_w, int fb_h,
                  Sprites &sprites, const Palette &palette) const {
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

    // Centered label, alpha=true, brightness=button.effectbrightness.
    int xoff = (static_cast<int>(width) -
                static_cast<int>(text.size()) * advance) /
               2;
    int textX = top_left_x + xoff;
    int textY = top_left_y + yoff;
    DrawText(fb, fb_w, fb_h, textX, textY, text, sprites,
             font_bank, advance, /*alpha=*/true, /*color=*/0,
             effectbrightness, palette);
}

}  // namespace silencer
