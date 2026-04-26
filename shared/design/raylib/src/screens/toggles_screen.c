#include "screens.h"
#include "../widgets/toggle.h"
#include "../font.h"
#include <raylib.h>
#include <stddef.h>

static sd_toggle_t agencies[5];
static sd_toggle_t boxes[3];
static bool inited = false;

void sd_screen_toggles(const sd_screen_ctx_t *ctx) {
    if (!inited) {
        const char *names[5] = { "Noxis", "Lazarus", "Caliber", "Static", "Blackrose" };
        for (int i = 0; i < 5; i++) {
            sd_toggle_init(&agencies[i], SD_TOG_AGENCY, 80 + i * 60, 120, i, 1, names[i]);
        }
        agencies[0].selected = true;

        for (int i = 0; i < 3; i++) {
            sd_toggle_init(&boxes[i], SD_TOG_CHECKBOX, 100 + i * 80, 250, 0, 0, NULL);
        }
        boxes[1].selected = true;
        inited = true;
    }

    sd_draw_text("TOGGLES", 10, 5, 135, 11, 0, 128);

    sd_draw_text("Agency icons (radio group, set=1) — click to select", 10, 80, 133, 6, 0, 128);
    for (int i = 0; i < 5; i++) {
        if (ctx->mouse_left_pressed && sd_toggle_inside(&agencies[i], ctx->mouse_x, ctx->mouse_y)) {
            for (int j = 0; j < 5; j++) agencies[j].selected = false;
            agencies[i].selected = true;
        }
        sd_toggle_draw(&agencies[i]);
    }

    sd_draw_text("Checkboxes (independent toggles)", 10, 220, 133, 6, 0, 128);
    for (int i = 0; i < 3; i++) {
        if (ctx->mouse_left_pressed && sd_toggle_inside(&boxes[i], ctx->mouse_x, ctx->mouse_y)) {
            boxes[i].selected = !boxes[i].selected;
        }
        sd_toggle_draw(&boxes[i]);
    }

    sd_draw_text("Selected agency: dim->bright (effectbrightness 32->128)", 10, 320, 133, 6, 0, 128);
    sd_draw_text("Checkbox: bank 7 indices 18 (checked) / 19 (unchecked)", 10, 334, 133, 6, 0, 128);
}
