#include "panel.h"
#include "../sprite.h"

/* DrawMessageBackground — bank 188, top + bottom only, right corners pinned at (x+w-36). */
void sd_draw_panel(int x, int y, int w, int h) {
    int sw0, sh0, sw1, sh1, sw2, sh2;
    int ox, oy;
    if (!sd_sprite_metrics(188, 0, &sw0, &sh0, &ox, &oy)) return;
    if (!sd_sprite_metrics(188, 1, &sw1, &sh1, &ox, &oy)) return;
    if (!sd_sprite_metrics(188, 2, &sw2, &sh2, &ox, &oy)) return;

    /* Top row */
    sd_sprite_draw_b(188, 0, x, y, 128);
    int tx = sw0;
    while (tx < w - sw2) {
        int clipw = w - tx - 36;
        if (clipw > sw1) clipw = sw1;
        if (clipw <= 0) break;
        /* For simplicity draw the full tile sprite and let the next iteration overlap. */
        sd_sprite_draw_b(188, 1, x + tx, y, 128);
        tx += clipw;
    }
    sd_sprite_draw_b(188, 2, x + w - 36, y, 128);

    /* Bottom row at y + h */
    sd_sprite_draw_b(188, 6, x, y + h, 128);
    tx = sw0;
    while (tx < w - sw2) {
        int clipw = w - tx - 36;
        if (clipw > sw1) clipw = sw1;
        if (clipw <= 0) break;
        sd_sprite_draw_b(188, 7, x + tx, y + h, 128);
        tx += clipw;
    }
    sd_sprite_draw_b(188, 8, x + w - 36, y + h, 128);
}
