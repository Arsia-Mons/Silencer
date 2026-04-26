#include "scrollbar.h"
#include "../sprite.h"
#include <string.h>

void sd_scrollbar_init(sd_scrollbar_t *s, int x, int y) {
    memset(s, 0, sizeof(*s));
    s->x = x; s->y = y;
    s->bank = 7;
    s->res_index = 9;
    s->bar_index = 10;
    s->draw = true;
}

void sd_scrollbar_draw(const sd_scrollbar_t *s) {
    if (!s->draw) return;
    sd_sprite_draw_b(s->bank, s->res_index, s->x, s->y, 128);
    int sw, sh, ox, oy;
    if (sd_sprite_metrics(s->bank, s->res_index, &sw, &sh, &ox, &oy)) {
        int track_top = s->y - oy + 16;
        int track_h = sh - 32;
        int frac_y = track_top;
        if (s->scrollmax > 0) {
            frac_y = track_top + (track_h * s->scroll) / s->scrollmax - 4;
        }
        sd_sprite_draw_b(s->bank, s->bar_index, s->x, frac_y, 128);
    }
}

void sd_scrollbar_up(sd_scrollbar_t *s) {
    if (s->scroll > 0) s->scroll--;
}
void sd_scrollbar_down(sd_scrollbar_t *s) {
    if (s->scroll < s->scrollmax) s->scroll++;
}

int sd_scrollbar_hit(const sd_scrollbar_t *s, int mx, int my) {
    int sw, sh, ox, oy;
    if (!sd_sprite_metrics(s->bank, s->res_index, &sw, &sh, &ox, &oy)) return -1;
    int x1 = s->x - ox;
    int y1 = s->y - oy;
    if (mx < x1 || mx > x1 + sw) return -1;
    if (my < y1 || my > y1 + sh) return -1;
    if (my < y1 + 16) return 0;
    if (my > y1 + sh - 16) return 2;
    return 1;
}
