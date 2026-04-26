#include "textinput.h"
#include "../font.h"
#include "../palette.h"
#include <raylib.h>
#include <string.h>

void sd_textinput_init(sd_textinput_t *t, int x, int y, int w, int h,
                       int bank, int fontwidth, int maxchars, int maxwidth,
                       bool password, bool numbersonly) {
    memset(t, 0, sizeof(*t));
    t->x = x; t->y = y; t->width = w; t->height = h;
    t->bank = bank ? bank : 135;
    t->fontwidth = fontwidth ? fontwidth : 9;
    t->maxchars = maxchars > 0 ? maxchars : 256;
    if (t->maxchars > (int)sizeof(t->text) - 1) t->maxchars = sizeof(t->text) - 1;
    t->maxwidth = maxwidth > 0 ? maxwidth : 10;
    t->caretcolor = 140;
    t->password = password;
    t->numbersonly = numbersonly;
}

void sd_textinput_char(sd_textinput_t *t, int c) {
    if (t->inactive) return;
    if (t->offset >= t->maxchars) return;
    if (t->numbersonly) {
        if (c < '0' || c > '9') return;
    } else {
        if (c < 0x20 || c > 0x7F) return;
    }
    if (t->offset >= t->maxwidth + t->scrolled) t->scrolled++;
    t->text[t->offset++] = (char)c;
    t->text[t->offset] = '\0';
}

void sd_textinput_backspace(sd_textinput_t *t) {
    if (t->inactive) return;
    if (t->offset > 0) {
        t->offset--;
        t->text[t->offset] = '\0';
        if (t->scrolled > 0) t->scrolled--;
    }
}

void sd_textinput_enter(sd_textinput_t *t) {
    if (t->inactive) return;
    t->enterpressed = true;
}

bool sd_textinput_inside(const sd_textinput_t *t, int mx, int my) {
    return mx > t->x && mx < t->x + t->width
        && my > t->y && my < t->y + t->height;
}

void sd_textinput_draw(const sd_textinput_t *t, int renderer_state_i) {
    const char *src = t->text + t->scrolled;
    char tmp[512];
    if (t->password) {
        int n = (int)strlen(src);
        if (n > (int)sizeof(tmp) - 1) n = sizeof(tmp) - 1;
        for (int i = 0; i < n; i++) tmp[i] = '*';
        tmp[n] = '\0';
        src = tmp;
    }
    uint8_t bright = t->inactive ? 64 : 128;
    sd_draw_text(src, t->x, t->y, t->bank, t->fontwidth, 0, bright);

    if (!t->inactive && t->showcaret && (renderer_state_i % 32 < 16)) {
        int len = (int)strlen(src);
        int cx = t->x + len * t->fontwidth;
        int cy = t->y - 1;
        int ch = (int)((float)t->height * 0.8f);
        Color cc = sd_palettes[0][t->caretcolor];
        DrawRectangle(cx, cy, 1, ch, cc);
    }
}
