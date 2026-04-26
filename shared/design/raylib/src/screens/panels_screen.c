#include "screens.h"
#include "../widgets/panel.h"
#include "../widgets/modal.h"
#include "../widgets/loading.h"
#include "../font.h"
#include <math.h>

void sd_screen_panels(const sd_screen_ctx_t *ctx) {
    sd_draw_text("PANELS, MODAL, LOADING BAR", 10, 5, 135, 11, 0, 128);

    sd_draw_text("Horizontal-stretch chat panel (bank 188, top+bottom rows):",
                 10, 35, 133, 6, 0, 128);
    sd_draw_panel(20, 60, 360, 30);
    sd_draw_text("(ALL): chat overlay text rendered here", 30, 70, 133, 6, 0, 136);

    sd_draw_text("Modal dialog (bank 40 idx 4) — Enter or click OK to dismiss:",
                 10, 130, 133, 6, 0, 128);
    /* Modal floats centered. Dimming background: not in design system. */
    sd_modal_draw("Could not create game", true,
                  ctx->mouse_x, ctx->mouse_y, ctx->mouse_left_pressed);

    /* Loading bar at the bottom */
    sd_draw_text("Loading bar (palette 123, 500x20 centered):",
                 10, 360, 133, 6, 0, 128);
    float prog = (float)((ctx->state_i % 96) / 96.0f);
    sd_loading_bar_draw(prog);
}
