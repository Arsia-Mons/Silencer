#include "hud.h"
#include "../sprite.h"
#include "../font.h"
#include "../palette.h"
#include <raylib.h>
#include <stdio.h>

/* Draw a sprite cropped vertically (bottom-up fill, y-grows-down). The fill is
 * the bottom `frac` portion of the sprite. */
static void draw_bar_sprite_v(int bank, int idx, int x, int y, float frac) {
    int sw, sh, ox, oy;
    if (!sd_sprite_metrics(bank, idx, &sw, &sh, &ox, &oy)) return;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int crop_top = sh - (int)(frac * sh);
    int dx = x - ox;
    int dy = y - oy + crop_top;
    /* For simplicity in this design hydration we just clip via raylib's scissor. */
    BeginScissorMode(dx, dy, sw, sh - crop_top);
    sd_sprite_draw_b(bank, idx, x, y, 128);
    EndScissorMode();
}

static void draw_bar_sprite_h(int bank, int idx, int x, int y, float frac) {
    int sw, sh, ox, oy;
    if (!sd_sprite_metrics(bank, idx, &sw, &sh, &ox, &oy)) return;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int dx = x - ox;
    int dy = y - oy;
    BeginScissorMode(dx, dy, (int)(frac * sw), sh);
    sd_sprite_draw_b(bank, idx, x, y, 128);
    EndScissorMode();
}

void sd_hud_draw(const sd_hud_state_t *s) {
    /* HUD frame */
    sd_sprite_draw_b(94, 0, 235, 419, 128);

    /* Health bar */
    float hp = s->max_health > 0 ? (float)s->health / s->max_health : 0;
    draw_bar_sprite_v(95, 0, 158, 463, hp);
    if (hp <= 0.5f && (s->state_i % 8) < 4) {
        sd_sprite_draw_b(95, 3, 158, 463, 128);
    }

    /* Shield bar */
    float sh_frac = s->max_shield > 0 ? (float)s->shield / s->max_shield : 0;
    draw_bar_sprite_v(95, 1, 481, 463, sh_frac);
    if (sh_frac <= 0.5f && (s->state_i % 8) < 4) {
        sd_sprite_draw_b(95, 4, 481, 463, 128);
    }

    /* Fuel bar — left to right */
    float fuel = s->max_fuel > 0 ? (float)s->fuel / s->max_fuel : 0;
    sd_sprite_draw_b(95, 5, 200, 463, 128);   /* fuel frame */
    draw_bar_sprite_h(95, 6, 200, 463, fuel);
    if (s->fuel_low) sd_sprite_draw_b(95, 8, 200, 463, 128);

    /* File progress */
    float files = s->max_files > 0 ? (float)s->files / s->max_files : 0;
    draw_bar_sprite_h(95, 7, 200, 470, files);

    /* Numbers */
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", s->health);
    sd_draw_tiny_text(buf, 158, 463, 161, 128);

    snprintf(buf, sizeof(buf), "%d", s->shield);
    sd_draw_tiny_text(buf, 481, 463, 202, 128);

    snprintf(buf, sizeof(buf), "%d", s->ammo);
    sd_draw_text(buf, 117, 457, 135, 12, 0, 128);

    snprintf(buf, sizeof(buf), "$%d", s->credits);
    sd_draw_text(buf, 572, 456, 135, 12, 202, 128);
}
