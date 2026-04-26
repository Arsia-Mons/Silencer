// Button widget — see docs/design/widget-button.md.
//
// Currently scoped to B196x33 only (the one variant the main menu uses).
// Adding more variants later means adding rows to kVariants in button.cpp
// and entries to ButtonType.
#pragma once

#include "widget.h"

namespace silencer {

enum class ButtonType {
    B196x33,
};

struct ButtonVariant {
    int width;
    int height;
    int sprite_bank;     // sprite bank for the chrome (frames base..base+4)
    int base_index;      // INACTIVE frame index in the bank
    unsigned text_bank;  // font bank for the label
    int text_advance;    // px between glyph origins
    int text_yoff;       // px down from sprite top-left to label top-left
};

const ButtonVariant& GetButtonVariant(ButtonType t);

class Button : public Widget {
   public:
    enum State : std::uint8_t { kInactive = 0, kActivating = 1, kDeactivating = 2, kActive = 3 };

    Button(ButtonType t, int x, int y, std::string text);

    void Tick() override;
    void Draw(const DrawCtx& ctx) override;
    bool HitTest(int mx, int my) const override;
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override;

    bool clicked = false;  // edge-triggered; caller resets each frame
    std::string text;
    ButtonType type;

   private:
    State state_ = kInactive;
    std::uint8_t state_i_ = 0;
};

}  // namespace silencer
