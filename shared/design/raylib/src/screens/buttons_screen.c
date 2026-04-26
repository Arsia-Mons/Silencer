#include "screens.h"
#include "../widgets/button.h"
#include "../font.h"

static sd_button_t buttons[7];
static bool inited = false;

void sd_screen_buttons(const sd_screen_ctx_t *ctx) {
    if (!inited) {
        sd_button_init(&buttons[0], SD_BTN_B112x33, 80,  60, "JOIN");
        sd_button_init(&buttons[1], SD_BTN_B196x33, 220, 60, "CREATE GAME");
        sd_button_init(&buttons[2], SD_BTN_B220x33, 60,  120, "WIDE BUTTON");
        sd_button_init(&buttons[3], SD_BTN_B236x27, 320, 120, "EXTRA WIDE");
        sd_button_init(&buttons[4], SD_BTN_B52x21,  80,  200, "OK");
        sd_button_init(&buttons[5], SD_BTN_B156x21, 200, 200, "Go Back");
        sd_button_init(&buttons[6], SD_BTN_BCHECKBOX, 400, 205, "");
        inited = true;
    }

    sd_draw_text("BUTTONS", 10, 5, 135, 11, 0, 128);
    sd_draw_text("Hover to see activation animation (4-tick ramp).", 10, 25, 133, 6, 0, 128);

    for (int i = 0; i < 7; i++) {
        bool inside = sd_button_inside(&buttons[i], ctx->mouse_x, ctx->mouse_y);
        sd_button_tick(&buttons[i], inside);
        sd_button_draw(&buttons[i]);
    }

    sd_draw_text("B112x33", 80,  100, 133, 6, 0, 128);
    sd_draw_text("B196x33", 220, 100, 133, 6, 0, 128);
    sd_draw_text("B220x33",  60, 160, 133, 6, 0, 128);
    sd_draw_text("B236x27", 320, 160, 133, 6, 0, 128);
    sd_draw_text("B52x21",   80, 230, 133, 6, 0, 128);
    sd_draw_text("B156x21", 200, 230, 133, 6, 0, 128);
    sd_draw_text("BCHECKBOX (click)", 380, 230, 133, 6, 0, 128);

    sd_draw_text("Variants share the same INACTIVE -> ACTIVATING -> ACTIVE state machine,",
                 10, 320, 133, 6, 0, 128);
    sd_draw_text("with brightness lerping 128 -> 136 over 4 ticks (168 ms at 23.8 Hz).",
                 10, 334, 133, 6, 0, 128);
}
