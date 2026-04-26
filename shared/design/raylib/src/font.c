#include "font.h"
#include "sprite.h"
#include "palette.h"
#include <string.h>

int sd_font_glyph_height(int bank) {
    switch (bank) {
        case 132: return 5;
        case 133: return 11;
        case 134: return 15;
        case 135: return 19;
        case 136: return 23;
        default:  return 11;
    }
}

static int ascii_offset(int bank) {
    return (bank == 132) ? 34 : 33;
}

void sd_draw_text(const char *text, int x, int y, int bank, int width,
                  int color_index, uint8_t brightness) {
    if (!text) return;
    int off = ascii_offset(bank);
    Color tint = (color_index != 0) ? sd_palettes[0][color_index] : (Color){0,0,0,0};
    int cx = x;
    for (const char *p = text; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ') { cx += width; continue; }
        if (c < 0x21 || c > 0x7F) { cx += width; continue; }
        int idx = c - off;
        if (idx < 0) { cx += width; continue; }
        sd_sprite_draw(bank, idx, cx, y, tint, brightness);
        cx += width;
    }
}

void sd_draw_tiny_text(const char *text, int x, int y, int color_index, uint8_t brightness) {
    if (!text) return;
    int len = (int)strlen(text);
    int width = 4;
    int total = len * width;
    int sx = x - total / 2;
    Color tint = (color_index != 0) ? sd_palettes[0][color_index] : (Color){0,0,0,0};
    int off = ascii_offset(132);
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        int dx = sx + i * width;
        if (c == '1') dx -= 1;
        if (c == ' ') continue;
        if (c < 0x22 || c > 0x7F) continue;
        int idx = c - off;
        sd_sprite_draw(132, idx, dx, y, tint, brightness);
    }
}

void sd_draw_text_shadowed(const char *text, int x, int y, int bank, int width,
                           int color_index, uint8_t brightness) {
    int sb = (int)brightness - 64;
    if (sb < 8) sb = 8;
    sd_draw_text(text, x + 1, y + 1, bank, width, color_index, (uint8_t)sb);
    sd_draw_text(text, x, y, bank, width, color_index, brightness);
}
