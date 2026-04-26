// TextInput — single-line, no cursor movement (per spec).
#pragma once

#include "widget.h"

#include <string>

namespace silencer {

class TextInput : public Widget {
   public:
    TextInput(int x, int y, int w, int h, unsigned bank = 133, int fontwidth = 6,
              int maxchars = 256, int maxwidth = 10);

    void Tick() override {}
    void Draw(const DrawCtx& ctx) override;
    bool HitTest(int mx, int my) const override;
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override;
    void OnKey(int sdl_keycode) override;
    void OnTextInput(const char* utf8) override;

    std::string text;
    int width = 100;
    int height = 14;
    unsigned res_text_bank = 133;
    int fontwidth = 6;
    int maxchars = 256;
    int maxwidth = 10;
    bool password = false;
    bool numbers_only = false;
    bool inactive = false;
    bool show_caret = false;
    int scrolled = 0;
    std::uint8_t caret_color = 140;
};

}  // namespace silencer
