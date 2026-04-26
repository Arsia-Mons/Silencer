#ifndef SD_TOGGLE_H
#define SD_TOGGLE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SD_TOG_AGENCY,   /* sprite bank 181 */
    SD_TOG_CHECKBOX, /* sprite bank 7   */
} sd_toggle_mode_t;

typedef struct {
    sd_toggle_mode_t mode;
    int              x, y;
    int              agency_index; /* 0..4 for agency mode */
    int              set;          /* radio group; 0 = standalone */
    bool             selected;
    char             text[64];
} sd_toggle_t;

void sd_toggle_init(sd_toggle_t *t, sd_toggle_mode_t mode, int x, int y,
                    int agency_index, int set, const char *label);

void sd_toggle_draw(const sd_toggle_t *t);
bool sd_toggle_inside(const sd_toggle_t *t, int mx, int my);

#endif
