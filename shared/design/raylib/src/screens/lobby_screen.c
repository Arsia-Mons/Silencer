#include "screens.h"
#include "../widgets/button.h"
#include "../widgets/toggle.h"
#include "../widgets/textinput.h"
#include "../widgets/textbox.h"
#include "../widgets/selectbox.h"
#include "../widgets/scrollbar.h"
#include "../font.h"
#include "../palette.h"
#include <raylib.h>
#include <stddef.h>

static sd_button_t b_back, b_create, b_join;
static sd_toggle_t agencies[5];
static sd_textinput_t chat_input;
static sd_textbox_t   chat_msgs, presence;
static sd_selectbox_t games;
static sd_scrollbar_t scroll;
static bool inited = false;

static void seed(void) {
    sd_button_init(&b_back,   SD_BTN_B156x21, 473, 29, "Go Back");
    sd_button_init(&b_create, SD_BTN_B156x21, 242, 68, "Create Game");
    sd_button_init(&b_join,   SD_BTN_B156x21, 436, 430, "Join Game");

    for (int i = 0; i < 5; i++) {
        sd_toggle_init(&agencies[i], SD_TOG_AGENCY, 20 + i * 42, 90, i, 1, NULL);
    }
    agencies[0].selected = true;

    sd_textinput_init(&chat_input, 18, 437, 360, 14, 133, 6, 200, 60, false, false);

    sd_textbox_init(&chat_msgs, 19, 220, 242, 207, 133, 6, 11, 256);
    sd_textbox_addline(&chat_msgs, "[server] Welcome to Silencer.", 0, 128, false);
    sd_textbox_addline(&chat_msgs, "[lobby] Player Noxis joined.", 200, 136, false);
    sd_textbox_addline(&chat_msgs, "<Lazarus> looking for a fight",  0, 136, false);
    sd_textbox_addline(&chat_msgs, "<Static> count me in",           0, 136, false);
    sd_textbox_addline(&chat_msgs, "[lobby] map -> Eastside",        129, 160, false);

    sd_textbox_init(&presence, 267, 220, 110, 207, 133, 6, 11, 256);
    sd_textbox_addline(&presence, "Noxis",     0, 128, false);
    sd_textbox_addline(&presence, "Lazarus",   0, 128, false);
    sd_textbox_addline(&presence, "Caliber",   0, 128, false);
    sd_textbox_addline(&presence, "Blackrose", 0, 128, false);
    sd_textbox_addline(&presence, "Static",    0, 128, false);

    sd_selectbox_init(&games, 407, 89, 214, 265, 14);
    const char *items[] = {
        "Eastside Combat",
        "Black Ops Arena",
        "Pickup Match #4",
        "TestGame12",
        "DroneNet Lobby",
    };
    int n_items = (int)(sizeof(items) / sizeof(items[0]));
    for (int i = 0; i < n_items; i++)
        sd_selectbox_add(&games, items[i], i);
    games.selecteditem = 0;

    sd_scrollbar_init(&scroll, 615, 89);
    scroll.scrollmax = 5;
}

void sd_screen_lobby(const sd_screen_ctx_t *ctx) {
    if (!inited) { seed(); inited = true; }

    /* Header */
    sd_draw_text("Silencer", 15, 32, 135, 11, 152, 128);
    sd_draw_text("v.00028",  115, 39, 133, 6, 189, 128);
    sd_draw_text("Eastside", 180, 32, 135, 11, 129, 160);

    /* Character panel labels */
    sd_draw_text("hvent90",  20, 71,  134, 8, 200, 128);
    sd_draw_text("Level: 12",         17, 130, 133, 7, 129, 160);
    sd_draw_text("Wins: 41",          17, 143, 133, 7, 129, 160);
    sd_draw_text("Losses: 27",        17, 156, 133, 7, 129, 160);
    sd_draw_text("End:9 Sh:7 Hk:5",   17, 169, 133, 7, 129, 160);

    /* Channel label and "Active Games" label */
    sd_draw_text("All",          15, 200, 134, 8, 0, 128);
    sd_draw_text("Active Games", 405, 70, 134, 8, 0, 128);

    /* Toggle agency icons + buttons */
    for (int i = 0; i < 5; i++) {
        if (ctx->mouse_left_pressed && sd_toggle_inside(&agencies[i], ctx->mouse_x, ctx->mouse_y)) {
            for (int j = 0; j < 5; j++) agencies[j].selected = false;
            agencies[i].selected = true;
        }
        sd_toggle_draw(&agencies[i]);
    }

    sd_button_t *btns[3] = { &b_back, &b_create, &b_join };
    for (int i = 0; i < 3; i++) {
        bool in = sd_button_inside(btns[i], ctx->mouse_x, ctx->mouse_y);
        sd_button_tick(btns[i], in);
        sd_button_draw(btns[i]);
    }

    sd_textbox_draw(&chat_msgs);
    sd_textbox_draw(&presence);

    if (ctx->mouse_left_pressed) sd_selectbox_click(&games, ctx->mouse_x, ctx->mouse_y);
    sd_selectbox_draw(&games);
    sd_scrollbar_draw(&scroll);

    /* Chat input — focused by default in this demo. */
    chat_input.showcaret = true;
    int c = GetCharPressed();
    while (c > 0) { sd_textinput_char(&chat_input, c); c = GetCharPressed(); }
    if (IsKeyPressed(KEY_BACKSPACE)) sd_textinput_backspace(&chat_input);
    sd_textinput_draw(&chat_input, ctx->state_i);

    /* Game info row sample */
    sd_draw_text("Map: eastside",   405, 358, 133, 6, 0, 128);
    sd_draw_text("Players: 3 / 8",  405, 370, 133, 6, 0, 128);
    sd_draw_text("Creator: Noxis",  405, 382, 133, 6, 0, 128);
    sd_draw_text("Lvl: 1-99",       405, 394, 133, 6, 0, 128);
    sd_draw_text("Mode: TDM",       405, 406, 133, 6, 0, 128);
}
