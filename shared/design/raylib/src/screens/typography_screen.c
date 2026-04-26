#include "screens.h"
#include "../font.h"

static const char *PANGRAM = "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789";
static const char *PUNCT   = "abcdefghijklmnopqrstuvwxyz !\"#$%&'()*+,-./:;<=>?@[]^_`{|}~";

void sd_screen_typography(const sd_screen_ctx_t *ctx) {
    sd_draw_text("TYPOGRAPHY", 10, 5, 135, 11, 0, 128);

    int y = 40;
    sd_draw_text("Bank 132 Tiny (advance 4)", 10, y, 133, 6, 0, 128);
    sd_draw_tiny_text("0123456789 HUD", 200, y, 0, 128);
    y += 30;

    sd_draw_text("Bank 133 Small (advance 6)", 10, y, 133, 6, 0, 128);
    sd_draw_text(PANGRAM, 10, y + 14, 133, 6, 0, 128);
    sd_draw_text(PUNCT,   10, y + 26, 133, 6, 0, 128);
    y += 60;

    sd_draw_text("Bank 134 Medium (advance 8)", 10, y, 133, 6, 0, 128);
    sd_draw_text(PANGRAM, 10, y + 14, 134, 8, 0, 128);
    y += 50;

    sd_draw_text("Bank 135 Large (advance 11)", 10, y, 133, 6, 0, 128);
    sd_draw_text(PANGRAM, 10, y + 14, 135, 11, 0, 128);
    y += 60;

    sd_draw_text("Bank 136 Extra-Large (advance 15)", 10, y, 133, 6, 0, 128);
    sd_draw_text("VICTORY",  10, y + 14, 136, 15, 0, 128);
    sd_draw_text("DEFEATED", 200, y + 14, 136, 15, 0, 128);
    y += 60;

    sd_draw_text("Brightness sweep (bank 135):", 10, y, 133, 6, 0, 128);
    int bx = 10;
    int bvals[] = { 32, 64, 96, 128, 136, 160, 192, 255 };
    for (int i = 0; i < 8; i++) {
        sd_draw_text("ABC", bx, y + 14, 135, 11, 0, (uint8_t)bvals[i]);
        bx += 50;
    }

    sd_draw_text("Tinted (semantic colors, bank 135):", 10, y + 50, 133, 6, 0, 128);
    int tints[] = { 152, 153, 161, 189, 200, 208, 224 };
    bx = 10;
    for (int i = 0; i < 7; i++) {
        sd_draw_text("ABC", bx, y + 64, 135, 11, tints[i], 128);
        bx += 50;
    }
}
