#ifndef SD_TEXTBOX_H
#define SD_TEXTBOX_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char    text[128];
    uint8_t color;
    uint8_t brightness;
} sd_textbox_line_t;

typedef struct {
    int   x, y, width, height;
    int   bank;
    int   fontwidth;
    int   lineheight;
    int   maxlines;
    int   scrolled;
    bool  bottomtotop;

    sd_textbox_line_t *lines;
    int lines_count;
    int lines_cap;
} sd_textbox_t;

void sd_textbox_init(sd_textbox_t *b, int x, int y, int w, int h,
                     int bank, int fontwidth, int lineheight, int maxlines);
void sd_textbox_free(sd_textbox_t *b);

void sd_textbox_addline(sd_textbox_t *b, const char *text,
                        uint8_t color, uint8_t brightness, bool scroll);

void sd_textbox_draw(const sd_textbox_t *b);

#endif
