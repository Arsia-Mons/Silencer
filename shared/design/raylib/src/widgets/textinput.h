#ifndef SD_TEXTINPUT_H
#define SD_TEXTINPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int      x, y, width, height;
    int      bank;        /* font bank, default 135 */
    int      fontwidth;   /* default 9 */
    int      maxchars;    /* buffer length cap */
    int      maxwidth;    /* visible char slots before scroll */
    int      caretcolor;  /* palette index, default 140 (yellow) */
    bool     password;
    bool     numbersonly;
    bool     inactive;
    bool     showcaret;   /* set by container focus */

    char     text[512];
    int      offset;      /* current length / caret pos (caret always at end) */
    int      scrolled;    /* characters scrolled off the left */

    bool     enterpressed;
    bool     tabpressed;
} sd_textinput_t;

void sd_textinput_init(sd_textinput_t *t, int x, int y, int w, int h,
                       int bank, int fontwidth, int maxchars, int maxwidth,
                       bool password, bool numbersonly);

/* Process a single character keypress (printable ASCII). */
void sd_textinput_char(sd_textinput_t *t, int c);
void sd_textinput_backspace(sd_textinput_t *t);
void sd_textinput_enter(sd_textinput_t *t);

/* Draw using the current state and the renderer's global tick counter (for caret blink). */
void sd_textinput_draw(const sd_textinput_t *t, int renderer_state_i);

bool sd_textinput_inside(const sd_textinput_t *t, int mx, int my);

#endif
