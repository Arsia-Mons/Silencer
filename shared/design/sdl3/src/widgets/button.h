#pragma once
#include <cstdint>
#include <string>

namespace silencer {

class Sprites;
class Palette;

enum class ButtonState : uint8_t {
    Inactive,
    Activating,
    Active,
    Deactivating,
};

struct Button {
    int16_t x = 0;
    int16_t y = 0;
    std::string text;
    uint8_t uid = 0;

    // B196x33 fixed values
    int16_t width = 196;
    int16_t height = 33;
    uint8_t res_bank = 6;
    uint8_t base_index = 7;
    uint8_t res_index = 7;
    uint8_t font_bank = 135;
    uint8_t advance = 11;
    int16_t yoff = 8;

    ButtonState state = ButtonState::Inactive;
    uint32_t state_i = 0;
    uint8_t effectbrightness = 128;
    bool clicked = false;

    // Hover/focus inputs (mouse_inside OR keyboard focus)
    void SetHovered(bool hovered);

    void Tick();

    void Draw(uint8_t *fb, int fb_w, int fb_h,
              Sprites &sprites, const Palette &palette) const;
};

}  // namespace silencer
