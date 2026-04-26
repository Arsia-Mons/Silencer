// Button — see §Components → Button (7 variants, state machine).
#pragma once

#include "widget.h"

namespace silencer {

enum class ButtonType {
    B112x33,
    B196x33,
    B220x33,
    B236x27,
    B52x21,
    B156x21,
    BCheckbox,
};

struct ButtonVariant {
    int width;
    int height;
    int sprite_bank;       // 0 = no sprite (B52x21)
    int base_index;
    unsigned text_bank;
    int text_advance;
    int text_yoff;
    int text_xoff_extra;   // +1 px for B52x21
    bool brightness_only;  // true for B156x21 (sprite frame doesn't change)
    bool no_anim;          // true for BCheckbox (no animation)
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

    bool clicked = false;  // edge-triggered, consume each frame
    std::string text;
    ButtonType type;

    // For BCheckbox.
    bool checked = false;

   private:
    State state_ = kInactive;
    std::uint8_t state_i_ = 0;
};

}  // namespace silencer
