// SelectBox — single-selection list, palette-180 highlight.
#pragma once

#include "widget.h"

#include <string>
#include <vector>

namespace silencer {

class SelectBox : public Widget {
   public:
    SelectBox(int x, int y, int w, int h, int lineheight = 13);

    void Draw(const DrawCtx& ctx) override;
    bool HitTest(int mx, int my) const override;
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override;
    void OnKey(int sdl_keycode) override;

    void AddItem(const std::string& s, int id = 0);

    int width = 200;
    int height = 100;
    int lineheight = 13;
    int selected_item = -1;
    int scrolled = 0;
    bool enter_pressed = false;
    std::vector<std::string> items;
    std::vector<int> item_ids;
};

}  // namespace silencer
