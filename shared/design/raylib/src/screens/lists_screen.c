#include "screens.h"
#include "../widgets/selectbox.h"
#include "../widgets/scrollbar.h"
#include "../widgets/textbox.h"
#include "../font.h"
#include <raylib.h>
#include <stdio.h>

static sd_selectbox_t games;
static sd_scrollbar_t scroll;
static sd_textbox_t   chat;
static bool inited = false;

void sd_screen_lists(const sd_screen_ctx_t *ctx) {
    if (!inited) {
        sd_selectbox_init(&games, 30, 80, 240, 200, 14);
        const char *items[] = {
            "TacticalArena_05", "BlackOpsLobby", "EastQuadrant",
            "TestGame12", "RustedYards", "QuickMatch_001",
            "MidnightStation", "DroneNet_v2", "PlayerLobbyA",
            "PlayerLobbyB", "PlayerLobbyC", "ScrollDemo_X",
        };
        int n = sizeof(items) / sizeof(items[0]);
        for (int i = 0; i < n; i++) sd_selectbox_add(&games, items[i], i);
        games.selecteditem = 0;

        sd_scrollbar_init(&scroll, 280, 80);
        scroll.scrollmax = 10;

        sd_textbox_init(&chat, 320, 80, 300, 200, 133, 6, 11, 256);
        sd_textbox_addline(&chat, "[server] Welcome to Silencer.", 0, 128, false);
        sd_textbox_addline(&chat, "[lobby] Player Noxis joined.", 200, 136, false);
        sd_textbox_addline(&chat, "[chat] hello world", 0, 136, false);
        sd_textbox_addline(&chat, "[chat] gg wp", 0, 136, false);
        sd_textbox_addline(&chat, "[lobby] Map changed to Eastside.", 129, 160, false);
        inited = true;
    }

    sd_draw_text("LISTS — SelectBox / ScrollBar / TextBox", 10, 5, 135, 11, 0, 128);

    sd_draw_text("SelectBox (palette 180 highlight):", 30, 60, 133, 6, 0, 128);
    if (ctx->mouse_left_pressed) sd_selectbox_click(&games, ctx->mouse_x, ctx->mouse_y);
    sd_selectbox_draw(&games);

    sd_draw_text("Scrollbar:", 280, 60, 133, 6, 0, 128);
    if (ctx->mouse_left_pressed) {
        int hit = sd_scrollbar_hit(&scroll, ctx->mouse_x, ctx->mouse_y);
        if (hit == 0) sd_scrollbar_up(&scroll);
        else if (hit == 2) sd_scrollbar_down(&scroll);
    }
    sd_scrollbar_draw(&scroll);

    sd_draw_text("TextBox (multi-line, per-line color/brightness):", 320, 60, 133, 6, 0, 128);
    sd_textbox_draw(&chat);
}
