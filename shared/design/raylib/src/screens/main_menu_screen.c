#include "screens.h"
#include "../widgets/button.h"
#include "../font.h"

static sd_button_t b_join, b_create, b_options, b_quit;
static bool inited = false;

void sd_screen_main_menu(const sd_screen_ctx_t *ctx) {
    if (!inited) {
        sd_button_init(&b_join,    SD_BTN_B196x33, 222, 200, "Connect to Lobby");
        sd_button_init(&b_create,  SD_BTN_B196x33, 222, 245, "Single Player");
        sd_button_init(&b_options, SD_BTN_B196x33, 222, 290, "Options");
        sd_button_init(&b_quit,    SD_BTN_B196x33, 222, 335, "Quit");
        inited = true;
    }

    /* Title */
    sd_draw_text("SILENCER", 220, 90, 136, 25, 152, 128); /* dark red, big */
    sd_draw_text("v.00028", 480, 100, 133, 6, 189, 128);

    sd_button_t *btns[4] = { &b_join, &b_create, &b_options, &b_quit };
    for (int i = 0; i < 4; i++) {
        bool inside = sd_button_inside(btns[i], ctx->mouse_x, ctx->mouse_y);
        sd_button_tick(btns[i], inside);
        sd_button_draw(btns[i]);
    }

    sd_draw_text("Composition: 4 stacked B196x33 buttons + title (bank 136 width 25, color 152)",
                 30, 440, 133, 6, 0, 128);
}
