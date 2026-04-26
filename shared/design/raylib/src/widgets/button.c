#include "button.h"
#include "../sprite.h"
#include "../font.h"
#include <string.h>
#include <stdio.h>
#include <raylib.h>

typedef struct {
    int  width, height;
    int  bank;        /* -1 for B52x21 (no sprite background) */
    int  base_index;
    int  text_bank;
    int  text_width;
    int  yoff;
    int  xoff_extra;
} btn_def_t;

static const btn_def_t BTN_DEFS[] = {
    [SD_BTN_B112x33]   = { 112, 33, 6,  28, 135, 11, 8, 0 },
    [SD_BTN_B196x33]   = { 196, 33, 6,   7, 135, 11, 8, 0 },
    [SD_BTN_B220x33]   = { 220, 33, 6,  23, 135, 11, 8, 0 },
    [SD_BTN_B236x27]   = { 236, 27, 6,   2, 135, 11, 8, 0 },
    [SD_BTN_B52x21]    = {  52, 21, -1,  0, 133,  7, 8, 1 },
    [SD_BTN_B156x21]   = { 156, 21, 7,  24, 134,  8, 4, 0 },
    [SD_BTN_BCHECKBOX] = {  13, 13, 7,  19,  -1,  0, 0, 0 },
};

void sd_button_init(sd_button_t *b, sd_btn_type_t type, int x, int y, const char *text) {
    memset(b, 0, sizeof(*b));
    b->type = type;
    b->x = x;
    b->y = y;
    if (text) {
        strncpy(b->text, text, sizeof(b->text) - 1);
    }
    b->state = SD_BTN_INACTIVE;
}

static int current_res_index(const sd_button_t *b) {
    const btn_def_t *d = &BTN_DEFS[b->type];
    if (b->type == SD_BTN_BCHECKBOX) {
        return b->checked ? 18 : 19;
    }
    if (b->type == SD_BTN_B52x21) return 0; /* unused */
    if (b->type == SD_BTN_B156x21) return d->base_index; /* fixed */
    int frame;
    switch (b->state) {
        case SD_BTN_ACTIVATING:   frame = b->state_i; break;
        case SD_BTN_ACTIVE:       frame = 4; break;
        case SD_BTN_DEACTIVATING: frame = 4 - b->state_i; break;
        default:                  frame = 0; break;
    }
    if (frame < 0) frame = 0;
    if (frame > 4) frame = 4;
    return d->base_index + frame;
}

static uint8_t current_brightness(const sd_button_t *b) {
    switch (b->state) {
        case SD_BTN_ACTIVATING:   return (uint8_t)(128 + b->state_i * 2);
        case SD_BTN_ACTIVE:       return 136;
        case SD_BTN_DEACTIVATING: return (uint8_t)(128 + (4 - b->state_i) * 2);
        default:                  return 128;
    }
}

void sd_button_tick(sd_button_t *b, bool mouse_inside) {
    /* Edge transitions */
    if (mouse_inside && b->state == SD_BTN_INACTIVE) {
        b->state = SD_BTN_ACTIVATING;
        b->state_i = 0;
    } else if (mouse_inside && b->state == SD_BTN_DEACTIVATING) {
        b->state = SD_BTN_ACTIVATING;
        b->state_i = 0;
    } else if (!mouse_inside && b->state == SD_BTN_ACTIVE) {
        b->state = SD_BTN_DEACTIVATING;
        b->state_i = 0;
    } else if (!mouse_inside && b->state == SD_BTN_ACTIVATING) {
        b->state = SD_BTN_DEACTIVATING;
        b->state_i = 0;
    }

    /* Per-tick advance */
    if (b->state == SD_BTN_ACTIVATING || b->state == SD_BTN_DEACTIVATING) {
        b->state_i++;
        if (b->state_i >= 4) {
            b->state = (b->state == SD_BTN_ACTIVATING) ? SD_BTN_ACTIVE : SD_BTN_INACTIVE;
            b->state_i = 0;
        }
    }
    b->prev_inside = mouse_inside;
}

void sd_button_bounds(const sd_button_t *b, int *x, int *y, int *w, int *h) {
    const btn_def_t *d = &BTN_DEFS[b->type];
    int ox = 0, oy = 0;
    if (d->bank >= 0) {
        int sw, sh;
        sd_sprite_metrics(d->bank, current_res_index(b), &sw, &sh, &ox, &oy);
    }
    if (x) *x = b->x - ox;
    if (y) *y = b->y - oy;
    if (w) *w = d->width;
    if (h) *h = d->height;
}

bool sd_button_inside(const sd_button_t *b, int mx, int my) {
    int x, y, w, h;
    sd_button_bounds(b, &x, &y, &w, &h);
    return mx > x && mx < x + w && my > y && my < y + h;
}

void sd_button_draw(const sd_button_t *b) {
    const btn_def_t *d = &BTN_DEFS[b->type];
    int idx = current_res_index(b);
    uint8_t bright = current_brightness(b);

    if (d->bank >= 0) {
        sd_sprite_draw_b(d->bank, idx, b->x, b->y, bright);
    } else {
        /* B52x21 — no sprite, but still has a hit area; draw nothing for the bg. */
    }

    if (d->text_bank > 0 && b->text[0]) {
        int len = (int)strlen(b->text);
        int xoff = (d->width - len * d->text_width) / 2 + d->xoff_extra;
        int ox = 0, oy = 0;
        if (d->bank >= 0) {
            int sw, sh;
            sd_sprite_metrics(d->bank, idx, &sw, &sh, &ox, &oy);
        }
        int tx = b->x - ox + xoff;
        int ty = b->y - oy + d->yoff;
        sd_draw_text(b->text, tx, ty, d->text_bank, d->text_width, 0, bright);
    }
}
