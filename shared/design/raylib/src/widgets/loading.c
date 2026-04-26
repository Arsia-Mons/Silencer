#include "loading.h"
#include "../palette.h"
#include <raylib.h>

void sd_loading_bar_draw(float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    int total = 500;
    int filled = (int)(progress * total);
    int x1 = (640 - total) / 2;       /* 70 */
    int y1 = (480 - 20) / 2;          /* 230 */
    int x2 = (640 + filled) / 2;      /* meets in the center growing to (640+500)/2=570 */
    int y2 = (480 + 20) / 2;          /* 250 */
    Color c = sd_palettes[0][123];
    DrawRectangle(x1, y1, x2 - x1, y2 - y1, c);
}
