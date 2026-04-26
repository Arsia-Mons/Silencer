#include "minimap.h"
#include "../sprite.h"
#include "../palette.h"
#include <raylib.h>

void sd_minimap_draw(int state_i) {
    /* Frame */
    sd_sprite_draw_b(94, 0, 235, 419, 128);
    /* Inner placeholder fill — this is normally a 172x62 paletted buffer. */
    Color c = sd_palettes[0][82]; /* dark blue */
    DrawRectangle(235, 419, 172, 62, c);

    /* Decorative blip */
    Color blip = sd_palettes[0][224];
    int bx = 235 + 86 + (state_i % 60) - 30;
    int by = 419 + 31;
    DrawCircle(bx, by, 2, blip);
}
