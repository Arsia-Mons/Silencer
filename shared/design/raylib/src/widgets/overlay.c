#include "overlay.h"
#include "../sprite.h"
#include "../font.h"
#include <string.h>

void sd_overlay_text_init(sd_overlay_t *o, int x, int y, const char *text,
                          int bank, int width, int color, uint8_t brightness) {
    memset(o, 0, sizeof(*o));
    o->x = x; o->y = y;
    if (text) strncpy(o->text, text, sizeof(o->text) - 1);
    o->textbank = bank ? bank : 135;
    o->textwidth = width ? width : 8;
    o->effectcolor = color;
    o->brightness = brightness ? brightness : 128;
}

void sd_overlay_sprite_init(sd_overlay_t *o, int x, int y, int bank, int index) {
    memset(o, 0, sizeof(*o));
    o->x = x; o->y = y;
    o->res_bank = bank;
    o->res_index = index;
    o->brightness = 128;
}

void sd_overlay_draw(const sd_overlay_t *o) {
    if (o->text[0]) {
        sd_draw_text(o->text, o->x, o->y, o->textbank, o->textwidth,
                     o->effectcolor, o->brightness);
    } else if (o->res_bank > 0) {
        sd_sprite_draw_b(o->res_bank, o->res_index, o->x, o->y, o->brightness);
    }
}
