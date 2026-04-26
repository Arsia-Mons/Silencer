#include "textbox.h"
#include "../font.h"
#include <stdlib.h>
#include <string.h>

void sd_textbox_init(sd_textbox_t *b, int x, int y, int w, int h,
                     int bank, int fontwidth, int lineheight, int maxlines) {
    memset(b, 0, sizeof(*b));
    b->x = x; b->y = y; b->width = w; b->height = h;
    b->bank = bank;
    b->fontwidth = fontwidth;
    b->lineheight = lineheight;
    b->maxlines = maxlines;
    b->lines_cap = 32;
    b->lines = (sd_textbox_line_t *)calloc(b->lines_cap, sizeof(sd_textbox_line_t));
}

void sd_textbox_free(sd_textbox_t *b) {
    if (b->lines) free(b->lines);
    b->lines = NULL;
    b->lines_count = 0;
}

void sd_textbox_addline(sd_textbox_t *b, const char *text,
                        uint8_t color, uint8_t brightness, bool scroll) {
    if (b->lines_count >= b->maxlines) {
        memmove(&b->lines[0], &b->lines[1], sizeof(sd_textbox_line_t) * (b->lines_count - 1));
        b->lines_count--;
    }
    if (b->lines_count == b->lines_cap) {
        b->lines_cap *= 2;
        b->lines = (sd_textbox_line_t *)realloc(b->lines, b->lines_cap * sizeof(sd_textbox_line_t));
    }

    int max_chars = b->fontwidth > 0 ? (b->width / b->fontwidth) : 16;
    if (max_chars > (int)sizeof(b->lines[0].text) - 1) max_chars = sizeof(b->lines[0].text) - 1;
    sd_textbox_line_t *L = &b->lines[b->lines_count++];
    int n = (int)strlen(text);
    if (n > max_chars) n = max_chars;
    memcpy(L->text, text, n);
    L->text[n] = '\0';
    L->color = color;
    L->brightness = brightness;

    if (scroll) {
        int visible = b->height / b->lineheight;
        if (b->lines_count > visible) b->scrolled = b->lines_count - visible;
        else                          b->scrolled = 0;
    }
}

void sd_textbox_draw(const sd_textbox_t *b) {
    int visible = b->height / b->lineheight;
    int end = b->scrolled + visible;
    if (end > b->lines_count) end = b->lines_count;
    for (int i = b->scrolled; i < end; i++) {
        const sd_textbox_line_t *L = &b->lines[i];
        sd_draw_text(L->text, b->x,
                     b->y + (i - b->scrolled) * b->lineheight,
                     b->bank, b->fontwidth, L->color, L->brightness);
    }
}
