#include "modal.h"
#include "../sprite.h"
#include "../font.h"
#include "button.h"
#include <string.h>

static sd_button_t s_ok = {0};
static bool s_ok_inited = false;

bool sd_modal_draw(const char *message, bool show_ok, int mx, int my, bool click) {
    /* Background plate — sprite is pre-centered via offsets, so x/y = 0. */
    sd_sprite_draw_b(40, 4, 320, 240, 128);

    int textwidth = 8;
    int len = (int)strlen(message);
    int tx = 320 - (len * textwidth) / 2;
    int ty = show_ok ? 200 : 218;
    sd_draw_text(message, tx, ty, 134, textwidth, 0, 128);

    if (!show_ok) return false;

    if (!s_ok_inited) {
        sd_button_init(&s_ok, SD_BTN_B156x21, 242, 230, "OK");
        s_ok_inited = true;
    }
    bool inside = sd_button_inside(&s_ok, mx, my);
    sd_button_tick(&s_ok, inside);
    sd_button_draw(&s_ok);

    bool fired = false;
    if (inside && click) {
        fired = true;
    }
    return fired;
}
