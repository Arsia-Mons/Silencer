#ifndef SD_SELECTBOX_H
#define SD_SELECTBOX_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int   x, y, width, height;
    int   lineheight;
    int   maxlines;
    int   selecteditem; /* -1 = none */
    int   scrolled;
    bool  enterpressed;

    char (*items)[64];
    int  *itemids;
    int   items_count;
    int   items_cap;
} sd_selectbox_t;

void sd_selectbox_init(sd_selectbox_t *s, int x, int y, int w, int h, int lineheight);
void sd_selectbox_free(sd_selectbox_t *s);
void sd_selectbox_add(sd_selectbox_t *s, const char *text, int id);

void sd_selectbox_draw(const sd_selectbox_t *s);

/* Returns clicked item index (>=0) or -1 if outside; selecteditem updates internally on hit. */
int  sd_selectbox_click(sd_selectbox_t *s, int mx, int my);

#endif
