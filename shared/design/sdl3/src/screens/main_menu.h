#pragma once
#include <string>
#include <vector>

#include "widgets/button.h"
#include "widgets/interface.h"
#include "widgets/overlay.h"

namespace silencer {

class Sprites;
class Palette;

struct MainMenu {
    Overlay background;
    Overlay logo;
    Overlay version;
    std::vector<Button> buttons;
    Interface iface;

    void Build(const std::string &version_string);

    void Tick() { iface.Tick(); }

    void Draw(uint8_t *fb, int fb_w, int fb_h,
              Sprites &sprites, const Palette &palette);
};

}  // namespace silencer
