#include "screens.h"
#include "../palette.h"
#include "../font.h"
#include <raylib.h>
#include <stdio.h>

static const struct { int idx; const char *name; } SEMANTIC[] = {
    {112,"Toggle Active"}, {114,"Hack Incomplete"}, {123,"Loading Bar"},
    {126,"Neutral Light"}, {128,"Deploy"}, {129,"Info Tint"},
    {140,"Caret"}, {146,"Health Dmg"}, {150,"Minimap"},
    {152,"Title"}, {153,"Red Alert"}, {161,"Health Val"},
    {189,"Version"}, {192,"Secret Drop"}, {194,"Shield Dmg"},
    {200,"User Info"}, {202,"Shield Val"}, {204,"Team Base"},
    {205,"Shield Stencil"}, {208,"Std Msg"}, {210,"Poison/Base"},
    {224,"Highlight"}, {180,"List Highlight"},
};

void sd_screen_palette(const sd_screen_ctx_t *ctx) {
    sd_draw_text("PALETTE — 8-bit indexed (Palette 0)", 10, 5, 135, 11, 0, 128);

    /* 7 ramp groups (16 levels each) starting at index 2. */
    const char *names[7] = { "Gray", "Yellow/Fire", "Red", "Brown/Tan", "Orange", "Blue", "Green" };
    int top = 35;
    for (int g = 0; g < 7; g++) {
        sd_draw_text(names[g], 10, top + g * 30, 133, 6, 0, 128);
        for (int level = 0; level < 16; level++) {
            int idx = g * 16 + level + 2;
            Color c = sd_palettes[0][idx];
            DrawRectangle(110 + level * 24, top + g * 30 - 4, 22, 22, c);
        }
    }

    /* Sky band */
    sd_draw_text("Sky (226-255)", 10, top + 7*30, 133, 6, 0, 128);
    for (int i = 0; i < 30; i++) {
        Color c = sd_palettes[0][226 + i];
        DrawRectangle(110 + i * 14, top + 7*30 - 4, 13, 22, c);
    }

    /* Semantic UI colors */
    sd_draw_text("Semantic UI Colors", 10, 290, 135, 11, 0, 128);
    int n = (int)(sizeof(SEMANTIC) / sizeof(SEMANTIC[0]));
    for (int i = 0; i < n; i++) {
        int col = i % 4;
        int row = i / 4;
        int sx = 10 + col * 160;
        int sy = 320 + row * 28;
        Color c = sd_palettes[0][SEMANTIC[i].idx];
        DrawRectangle(sx, sy, 24, 20, c);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d %s", SEMANTIC[i].idx, SEMANTIC[i].name);
        sd_draw_text(buf, sx + 30, sy + 4, 133, 6, 0, 128);
    }
}
