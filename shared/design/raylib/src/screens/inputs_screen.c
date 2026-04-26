#include "screens.h"
#include "../widgets/textinput.h"
#include "../font.h"
#include <raylib.h>

static sd_textinput_t name_field;
static sd_textinput_t pwd_field;
static sd_textinput_t num_field;
static int focused = 0;
static bool inited = false;

static void focus(int i) {
    name_field.showcaret = (i == 0);
    pwd_field.showcaret  = (i == 1);
    num_field.showcaret  = (i == 2);
    focused = i;
}

void sd_screen_inputs(const sd_screen_ctx_t *ctx) {
    if (!inited) {
        sd_textinput_init(&name_field, 200, 80, 180, 14, 133, 6, 16, 16, false, false);
        sd_textinput_init(&pwd_field,  200, 130, 180, 14, 133, 6, 28, 28, true, false);
        sd_textinput_init(&num_field,  200, 180, 60, 20, 134, 8, 4, 50, false, true);
        focus(0);
        inited = true;
    }

    sd_draw_text("TEXTINPUT", 10, 5, 135, 11, 0, 128);

    sd_draw_text("Username:", 60, 82, 134, 8, 0, 128);
    sd_draw_text("Password:", 60, 132, 134, 8, 0, 128);
    sd_draw_text("Number:",   60, 182, 134, 8, 0, 128);

    /* Click to focus */
    if (ctx->mouse_left_pressed) {
        if (sd_textinput_inside(&name_field, ctx->mouse_x, ctx->mouse_y)) focus(0);
        else if (sd_textinput_inside(&pwd_field, ctx->mouse_x, ctx->mouse_y)) focus(1);
        else if (sd_textinput_inside(&num_field, ctx->mouse_x, ctx->mouse_y)) focus(2);
    }

    /* Tab to advance */
    if (IsKeyPressed(KEY_TAB)) focus((focused + 1) % 3);

    sd_textinput_t *fields[3] = { &name_field, &pwd_field, &num_field };
    sd_textinput_t *active = fields[focused];

    int c = GetCharPressed();
    while (c > 0) {
        sd_textinput_char(active, c);
        c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) sd_textinput_backspace(active);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) sd_textinput_enter(active);

    sd_textinput_draw(&name_field, ctx->state_i);
    sd_textinput_draw(&pwd_field, ctx->state_i);
    sd_textinput_draw(&num_field, ctx->state_i);

    sd_draw_text("Caret blinks at 32-tick period (16 on / 16 off, ~672 ms each).",
                 10, 250, 133, 6, 0, 128);
    sd_draw_text("Caret palette index 140 (#FCFC00). Tab cycles. Backspace deletes.",
                 10, 264, 133, 6, 0, 128);
    sd_draw_text("Password masks each char as '*'. Numbers field accepts only 0-9.",
                 10, 278, 133, 6, 0, 128);
}
