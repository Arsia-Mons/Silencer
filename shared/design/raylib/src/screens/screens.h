#ifndef SD_SCREENS_H
#define SD_SCREENS_H

#include <stdbool.h>

typedef struct {
    int  state_i;       /* renderer 23.8 Hz tick counter */
    int  mouse_x;
    int  mouse_y;
    bool mouse_left_pressed;
    bool mouse_left_down;
} sd_screen_ctx_t;

void sd_screen_palette(const sd_screen_ctx_t *ctx);
void sd_screen_typography(const sd_screen_ctx_t *ctx);
void sd_screen_buttons(const sd_screen_ctx_t *ctx);
void sd_screen_toggles(const sd_screen_ctx_t *ctx);
void sd_screen_inputs(const sd_screen_ctx_t *ctx);
void sd_screen_lists(const sd_screen_ctx_t *ctx);
void sd_screen_panels(const sd_screen_ctx_t *ctx);
void sd_screen_main_menu(const sd_screen_ctx_t *ctx);
void sd_screen_lobby(const sd_screen_ctx_t *ctx);
void sd_screen_hud(const sd_screen_ctx_t *ctx);
void sd_screen_buy_menu(const sd_screen_ctx_t *ctx);

#endif
