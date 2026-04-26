// ScrollBar — vertical with up/down arrows.
#pragma once

#include "widget.h"

namespace silencer {

class ScrollBar : public Widget {
   public:
    ScrollBar(int x, int y);

    void Draw(const DrawCtx& ctx) override;
    bool HitTest(int mx, int my) const override;
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override;

    int scroll_position = 0;
    int scroll_max = 100;
    std::uint8_t bar_index = 10;

    void ScrollUp() { if (scroll_position > 0) --scroll_position; }
    void ScrollDown() { if (scroll_position < scroll_max) ++scroll_position; }
};

}  // namespace silencer
