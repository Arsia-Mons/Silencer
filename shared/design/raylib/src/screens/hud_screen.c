#include "screens.h"
#include "../widgets/hud.h"
#include "../widgets/minimap.h"
#include "../widgets/panel.h"
#include "../font.h"

void sd_screen_hud(const sd_screen_ctx_t *ctx) {
    sd_draw_text("IN-GAME HUD COMPOSITION", 10, 5, 135, 11, 0, 128);

    sd_minimap_draw(ctx->state_i);

    sd_hud_state_t s = {
        .health = 73, .max_health = 100,
        .shield = 40, .max_shield = 100,
        .fuel   = 250, .max_fuel  = 400,
        .files  = 3,   .max_files = 8,
        .ammo   = 24, .credits = 1850,
        .fuel_low = false,
        .state_i = ctx->state_i,
    };
    sd_hud_draw(&s);

    /* Chat overlay using horizontal-stretch panel */
    sd_draw_panel(400, 280, 231, 30);
    sd_draw_text("<Noxis> regrouping at base", 410, 290, 133, 6, 0, 136);
    sd_draw_text("<Caliber> on my way",        410, 300, 133, 6, 0, 136);

    /* Top message */
    sd_draw_text("Defend the secret room!", 200, 10, 133, 7, 0, 128);

    /* Status messages stack */
    sd_draw_text("Noxis fragged Lazarus",      200, 360, 133, 7, 161, 128);
    sd_draw_text("You picked up: Energy Cell", 200, 350, 133, 7, 192, 128);

    /* Announcement */
    sd_draw_text("REINFORCEMENTS DEPLOYED", 130, 60, 135, 11, 208, 160);
}
