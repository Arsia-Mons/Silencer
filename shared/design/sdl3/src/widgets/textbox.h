// TextBox — multi-line read-only text area.
#pragma once

#include "widget.h"

#include <string>
#include <vector>

namespace silencer {

class TextBox : public Widget {
   public:
    TextBox(int x, int y, int w, int h);

    void Draw(const DrawCtx& ctx) override;

    void AddLine(const std::string& s, std::uint8_t color = 0,
                 std::uint8_t brightness = 128, bool scroll = true);

    int width = 100;
    int height = 100;
    unsigned res_text_bank = 133;
    int lineheight = 11;
    int fontwidth = 6;
    int maxlines = 256;
    int scrolled = 0;

    struct Line {
        std::string text;
        std::uint8_t color;
        std::uint8_t brightness;
    };
    std::vector<Line> lines;
};

}  // namespace silencer
