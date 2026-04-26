#include "screens/main_menu.h"

#include "palette.h"
#include "sprite.h"

namespace silencer {

void MainMenu::Build(const std::string &version_string) {
    // Background: bank 6 idx 0 at (0, 0).
    background = {};
    background.x = 0;
    background.y = 0;
    background.res_bank = 6;
    background.res_index = 0;

    // Logo: bank 208, animated. Initial idx 29, position (0, 0).
    logo = {};
    logo.x = 0;
    logo.y = 0;
    logo.res_bank = 208;
    logo.res_index = 29;
    logo.state_i = 0;

    // Version: text mode, bank 133, advance 11, position (10, 463).
    version = {};
    version.x = 10;
    version.y = 463;
    version.text = "Silencer v" + version_string;
    version.textbank = 133;
    version.textwidth = 11;

    // Buttons in draw order: Tutorial, Connect To Lobby, Options, Exit.
    buttons.clear();
    buttons.reserve(4);
    auto add_button = [&](const char *t, int16_t bx, int16_t by, uint8_t uid) {
        Button b{};
        b.text = t;
        b.x = bx;
        b.y = by;
        b.uid = uid;
        buttons.push_back(b);
    };
    add_button("Tutorial", 40, -134, 0);
    add_button("Connect To Lobby", 80, -67, 1);
    add_button("Options", 40, 0, 2);
    add_button("Exit", 0, 67, 3);

    // Interface wiring.
    iface.overlays = {&background, &logo, &version};
    iface.buttons.clear();
    for (auto &b : buttons) iface.buttons.push_back(&b);
    iface.activeobject = -1;
    iface.buttonenter = -1;
    iface.buttonescape = 3;  // Exit
}

void MainMenu::Draw(uint8_t *fb, int fb_w, int fb_h,
                    Sprites &sprites, const Palette &palette) {
    background.Draw(fb, fb_w, fb_h, sprites, palette);
    logo.Draw(fb, fb_w, fb_h, sprites, palette);
    version.Draw(fb, fb_w, fb_h, sprites, palette);
    for (auto &b : buttons) {
        b.Draw(fb, fb_w, fb_h, sprites, palette);
    }
}

}  // namespace silencer
