#include "toggle.h"
#include "../sprite.h"
#include "../palette.h"
#include "../font.h"
#include <string.h>

void sd_toggle_init(sd_toggle_t *t, sd_toggle_mode_t mode, int x, int y,
                    int agency_index, int set, const char *label) {
    memset(t, 0, sizeof(*t));
    t->mode = mode;
    t->x = x;
    t->y = y;
    t->agency_index = agency_index;
    t->set = set;
    if (label) strncpy(t->text, label, sizeof(t->text) - 1);
}

static void agency_metrics(const sd_toggle_t *t, int *bank, int *index) {
    *bank = 181;
    *index = t->agency_index;
}

static void checkbox_metrics(const sd_toggle_t *t, int *bank, int *index) {
    *bank = 7;
    *index = t->selected ? 18 : 19;
}

void sd_toggle_draw(const sd_toggle_t *t) {
    int bank, index;
    if (t->mode == SD_TOG_AGENCY) {
        agency_metrics(t, &bank, &index);
        Color tint = sd_palettes[0][112]; /* toggle active color */
        uint8_t bright = t->selected ? 128 : 32;
        sd_sprite_draw(bank, index, t->x, t->y, tint, bright);
    } else {
        checkbox_metrics(t, &bank, &index);
        sd_sprite_draw_b(bank, index, t->x, t->y, 128);
    }

    if (t->text[0]) {
        int len = (int)strlen(t->text);
        int tx = t->x - (len * 9) / 2;
        sd_draw_text(t->text, tx, t->y, 134, 9, 0, 128);
    }
}

bool sd_toggle_inside(const sd_toggle_t *t, int mx, int my) {
    int bank, index;
    if (t->mode == SD_TOG_AGENCY) agency_metrics(t, &bank, &index);
    else                          checkbox_metrics(t, &bank, &index);
    int sw, sh, ox, oy;
    if (!sd_sprite_metrics(bank, index, &sw, &sh, &ox, &oy)) return false;
    int x1 = t->x - ox;
    int y1 = t->y - oy;
    return mx > x1 && mx < x1 + sw && my > y1 && my < y1 + sh;
}
