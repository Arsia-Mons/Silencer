#ifndef SD_BUTTON_H
#define SD_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SD_BTN_B112x33,
    SD_BTN_B196x33,
    SD_BTN_B220x33,
    SD_BTN_B236x27,
    SD_BTN_B52x21,
    SD_BTN_B156x21,
    SD_BTN_BCHECKBOX,
} sd_btn_type_t;

typedef enum {
    SD_BTN_INACTIVE     = 0,
    SD_BTN_ACTIVATING   = 1,
    SD_BTN_DEACTIVATING = 2,
    SD_BTN_ACTIVE       = 3,
} sd_btn_state_t;

typedef struct {
    sd_btn_type_t type;
    int           x, y;
    char          text[64];
    bool          checked;     /* for BCHECKBOX */
    /* runtime */
    sd_btn_state_t state;
    int            state_i;    /* per-instance, ticks at 23.8 Hz */
    bool           clicked;    /* set true on click; consumer must clear */
    bool           prev_inside;
} sd_button_t;

void sd_button_init(sd_button_t *b, sd_btn_type_t type, int x, int y, const char *text);

/* Update animation state. mouse_inside reflects current hit-test result. */
void sd_button_tick(sd_button_t *b, bool mouse_inside);

/* Render. */
void sd_button_draw(const sd_button_t *b);

/* Hit-test the button at logical mouse coords. */
bool sd_button_inside(const sd_button_t *b, int mx, int my);

/* Get bounding box in logical pixels. */
void sd_button_bounds(const sd_button_t *b, int *x, int *y, int *w, int *h);

#endif
