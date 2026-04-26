// Overlay — sprite or text label, plus animated sprite banks.
#pragma once

#include "widget.h"

#include <string>
#include <vector>

namespace silencer {

class Overlay : public Widget {
   public:
    Overlay() = default;

    void Tick() override;
    void Draw(const DrawCtx& ctx) override;
    bool HitTest(int mx, int my) const override;
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override;

    // Text mode: when text is non-empty.
    std::string text;
    unsigned text_bank = 135;
    int text_width = 8;
    int text_lineheight = 10;
    bool text_color_ramp = false;
    bool text_allow_newline = false;
    bool draw_alpha = false;
    bool clicked = false;

    std::uint32_t state_i_anim = 0;  // for sprite-bank auto-animations
    bool destroyed = false;
};

}  // namespace silencer
