#ifndef SD_SCROLLBAR_H
#define SD_SCROLLBAR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int  x, y;
    int  bank;        /* default 7 */
    int  res_index;   /* track sprite, default 9 */
    int  bar_index;   /* thumb sprite, default 10 */
    int  scroll;
    int  scrollmax;
    bool draw;
} sd_scrollbar_t;

void sd_scrollbar_init(sd_scrollbar_t *s, int x, int y);
void sd_scrollbar_draw(const sd_scrollbar_t *s);

void sd_scrollbar_up(sd_scrollbar_t *s);
void sd_scrollbar_down(sd_scrollbar_t *s);

/* Returns: -1 outside, 0 = up arrow, 1 = track, 2 = down arrow. */
int sd_scrollbar_hit(const sd_scrollbar_t *s, int mx, int my);

#endif
