// Toggle — agency-icon (bank 181) or checkbox (bank 7) modes; radio sets.
#pragma once

#include "widget.h"

#include <string>

namespace silencer {

enum class ToggleMode { Agency, Checkbox };

class Toggle : public Widget {
   public:
    Toggle(ToggleMode mode, int x, int y, std::uint8_t set_id = 0,
           std::uint8_t agency_index = 0, std::string label = "");

    void Tick() override {}
    void Draw(const DrawCtx& ctx) override;
    bool HitTest(int mx, int my) const override;
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override;

    bool selected = false;
    std::uint8_t set_id = 0;     // mutual exclusion (radio) group
    std::uint8_t agency_index = 0;  // 0..4 for agency mode (bank 181 sprite index)
    std::string label;
    ToggleMode mode;
};

}  // namespace silencer
