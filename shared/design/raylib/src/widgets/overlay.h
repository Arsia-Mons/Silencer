#ifndef SD_OVERLAY_H
#define SD_OVERLAY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int   x, y;
    char  text[256];
    int   textbank;
    int   textwidth;
    int   res_bank;
    int   res_index;
    int   effectcolor;
    uint8_t brightness;
} sd_overlay_t;

void sd_overlay_text_init(sd_overlay_t *o, int x, int y, const char *text,
                          int bank, int width, int color, uint8_t brightness);
void sd_overlay_sprite_init(sd_overlay_t *o, int x, int y, int bank, int index);
void sd_overlay_draw(const sd_overlay_t *o);

#endif
