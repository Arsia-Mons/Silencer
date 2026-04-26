#include "selectbox.h"
#include "../font.h"
#include "../palette.h"
#include <raylib.h>
#include <stdlib.h>
#include <string.h>

void sd_selectbox_init(sd_selectbox_t *s, int x, int y, int w, int h, int lineheight) {
    memset(s, 0, sizeof(*s));
    s->x = x; s->y = y; s->width = w; s->height = h;
    s->lineheight = lineheight > 0 ? lineheight : 13;
    s->maxlines = 256;
    s->selecteditem = -1;
    s->items_cap = 16;
    s->items = (char (*)[64])calloc(s->items_cap, 64);
    s->itemids = (int *)calloc(s->items_cap, sizeof(int));
}

void sd_selectbox_free(sd_selectbox_t *s) {
    if (s->items) free(s->items);
    if (s->itemids) free(s->itemids);
    s->items = NULL; s->itemids = NULL; s->items_count = 0;
}

void sd_selectbox_add(sd_selectbox_t *s, const char *text, int id) {
    if (s->items_count == s->items_cap) {
        s->items_cap *= 2;
        s->items = (char (*)[64])realloc(s->items, s->items_cap * 64);
        s->itemids = (int *)realloc(s->itemids, s->items_cap * sizeof(int));
    }
    strncpy(s->items[s->items_count], text, 63);
    s->items[s->items_count][63] = '\0';
    s->itemids[s->items_count] = id;
    s->items_count++;
    int visible = s->height / s->lineheight;
    if (s->items_count > visible) s->scrolled = s->items_count - visible;
    else                          s->scrolled = 0;
}

void sd_selectbox_draw(const sd_selectbox_t *s) {
    int visible = s->height / s->lineheight;
    int end = s->scrolled + visible;
    if (end > s->items_count) end = s->items_count;
    Color hl = sd_palettes[0][180];
    for (int i = s->scrolled; i < end; i++) {
        int line = i - s->scrolled;
        int ly = s->y + line * s->lineheight;
        if (i == s->selecteditem) {
            DrawRectangle(s->x, ly, s->width, 11, hl);
        }
        sd_draw_text(s->items[i], s->x, ly, 133, 6, 0, 128);
    }
}

int sd_selectbox_click(sd_selectbox_t *s, int mx, int my) {
    int x1 = s->x;
    int x2 = s->x + s->width - 16;
    int y1 = s->y;
    int y2 = s->y + s->height;
    if (mx > x1 && mx < x2 && my > y1 && my < y2) {
        int idx = ((my - y1) / s->lineheight) + s->scrolled;
        if (idx < s->items_count) {
            s->selecteditem = idx;
            return idx;
        }
    }
    return -1;
}
