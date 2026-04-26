#include "screens.h"
#include "../sprite.h"
#include "../font.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

static const char *ITEMS[5] = {
    "Energy Cell",
    "Light Armor",
    "Frag Grenade",
    "EMP Module",
    "Stealth Cloak",
};
static const int PRICES[5] = { 150, 400, 250, 600, 900 };

void sd_screen_buy_menu(const sd_screen_ctx_t *ctx) {
    sd_draw_text("BUY MENU (in-game tech / item interface)", 10, 5, 135, 11, 0, 128);

    /* Background frame */
    sd_sprite_draw_b(102, 0, 160, 130, 128);

    /* Selection highlight pulse */
    int sel = (ctx->state_i / 30) % 5;
    int phase = ctx->state_i % 16;
    int bright;
    if (phase >= 8) bright = 128 + (phase % 8);
    else            bright = 128 + (8 - (phase % 8));

    /* Highlight sprite behind selected row */
    sd_sprite_draw_b(102, 1, 169, 139 + sel * 25, (uint8_t)bright);

    for (int i = 0; i < 5; i++) {
        int yoff = i * 25;
        uint8_t b = (i == sel) ? (uint8_t)bright : 128;
        sd_draw_text(ITEMS[i], 222, 145 + yoff, 134, 9, 0, b);

        char price[16];
        snprintf(price, sizeof(price), "$%d", PRICES[i]);
        int len = (int)strlen(price);
        int px = 440 - (len * 9) / 2;
        sd_draw_text(price, px, 145 + yoff, 134, 9, 0, b);
    }

    /* Up/Down arrows */
    sd_sprite_draw_b(102, 2, 460, 145, 128);
    sd_sprite_draw_b(102, 3, 460, 245, 128);

    /* Available credits */
    char credits[32];
    snprintf(credits, sizeof(credits), "Credits: $%d", 1850);
    int len = (int)strlen(credits);
    int cx = 320 - (len * 9) / 2;
    sd_draw_text(credits, cx, 275, 134, 9, 202, 128);
}
