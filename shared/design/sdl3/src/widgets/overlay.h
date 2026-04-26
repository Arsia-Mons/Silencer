#pragma once
#include <cstdint>
#include <string>

namespace silencer {

class Sprites;
class Palette;

struct Overlay {
    int16_t x = 0;
    int16_t y = 0;
    uint8_t res_bank = 0xFF;
    uint8_t res_index = 0;
    uint32_t state_i = 0;
    std::string text;
    uint8_t textbank = 135;
    uint8_t textwidth = 8;
    bool drawalpha = false;
    uint8_t effectcolor = 0;
    uint8_t effectbrightness = 128;
    bool textcolorramp = false;
    bool textallownewline = false;
    int32_t textlineheight = 10;

    // Per-bank Tick — drives res_index when applicable.
    void Tick();

    // Draw onto an indexed framebuffer. Camera offset is 0 on the menu.
    void Draw(uint8_t *fb, int fb_w, int fb_h,
              Sprites &sprites, const Palette &palette) const;
};

}  // namespace silencer
