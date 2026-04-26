/* Silencer Design System — Raylib hydration
 *
 * Renders the design system to a fixed 640x480 logical surface (RenderTexture2D)
 * and scales it to the actual window with point sampling.
 *
 * Use Left/Right arrow keys to cycle demo screens. Esc to quit.
 */

#include <raylib.h>
#include <stdio.h>
#include <string.h>

#include "palette.h"
#include "sprite.h"
#include "font.h"
#include "screens/screens.h"

#define LOGICAL_W 640
#define LOGICAL_H 480
#define WINDOW_W  1280
#define WINDOW_H  960
#define TICK_MS   42  /* ~23.8 Hz */

typedef struct {
    const char *name;
    void (*draw)(const sd_screen_ctx_t *);
} demo_t;

static demo_t SCREENS[] = {
    { "Palette",       sd_screen_palette },
    { "Typography",    sd_screen_typography },
    { "Buttons",       sd_screen_buttons },
    { "Toggles",       sd_screen_toggles },
    { "TextInput",     sd_screen_inputs },
    { "Lists",         sd_screen_lists },
    { "Panels/Modal",  sd_screen_panels },
    { "Main Menu",     sd_screen_main_menu },
    { "Lobby",         sd_screen_lobby },
    { "In-Game HUD",   sd_screen_hud },
    { "Buy Menu",      sd_screen_buy_menu },
};
#define N_SCREENS ((int)(sizeof(SCREENS) / sizeof(SCREENS[0])))

int main(int argc, char **argv) {
    const char *assets = (argc > 1) ? argv[1] : "../../assets";

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_W, WINDOW_H, "Silencer Design System (Raylib hydration)");
    SetTargetFPS(60);

    char palette_path[1024];
    snprintf(palette_path, sizeof(palette_path), "%s/PALETTE.BIN", assets);
    if (!sd_palette_load(palette_path)) {
        TraceLog(LOG_ERROR, "Cannot load palette at %s", palette_path);
    }
    if (!sd_sprites_load(assets)) {
        TraceLog(LOG_ERROR, "Cannot load sprites from %s", assets);
    }

    RenderTexture2D target = LoadRenderTexture(LOGICAL_W, LOGICAL_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    int screen_idx = 0;
    int tick_accum = 0;
    int state_i = 0;
    double last = GetTime();

    sd_screen_ctx_t ctx = {0};

    while (!WindowShouldClose()) {
        double now = GetTime();
        int elapsed_ms = (int)((now - last) * 1000.0);
        last = now;
        tick_accum += elapsed_ms;
        while (tick_accum >= TICK_MS) {
            state_i++;
            tick_accum -= TICK_MS;
        }
        ctx.state_i = state_i;

        /* Mouse coords in logical space */
        int win_w = GetScreenWidth();
        int win_h = GetScreenHeight();
        Vector2 m = GetMousePosition();
        /* Letterbox-aware: compute scale and offset same as draw step */
        float scale = (float)win_w / LOGICAL_W;
        float scale_y = (float)win_h / LOGICAL_H;
        if (scale_y < scale) scale = scale_y;
        int draw_w = (int)(LOGICAL_W * scale);
        int draw_h = (int)(LOGICAL_H * scale);
        int draw_x = (win_w - draw_w) / 2;
        int draw_y = (win_h - draw_h) / 2;
        ctx.mouse_x = (int)((m.x - draw_x) / scale);
        ctx.mouse_y = (int)((m.y - draw_y) / scale);
        ctx.mouse_left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        ctx.mouse_left_down    = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        /* Navigator */
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_PAGE_DOWN)) {
            screen_idx = (screen_idx + 1) % N_SCREENS;
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_PAGE_UP)) {
            screen_idx = (screen_idx - 1 + N_SCREENS) % N_SCREENS;
        }
        if (IsKeyPressed(KEY_HOME)) screen_idx = 0;

        /* Render to logical surface */
        BeginTextureMode(target);
        ClearBackground((Color){ 12, 12, 16, 255 });

        SCREENS[screen_idx].draw(&ctx);

        /* Footer chrome */
        char footer[160];
        snprintf(footer, sizeof(footer), "[%d/%d] %s   <- ->  to cycle    ESC to quit",
                 screen_idx + 1, N_SCREENS, SCREENS[screen_idx].name);
        DrawRectangle(0, LOGICAL_H - 14, LOGICAL_W, 14, (Color){0, 0, 0, 200});
        sd_draw_text(footer, 4, LOGICAL_H - 12, 133, 6, 0, 160);

        EndTextureMode();

        /* Present scaled */
        BeginDrawing();
        ClearBackground(BLACK);
        Rectangle src = { 0, 0, (float)LOGICAL_W, -(float)LOGICAL_H }; /* flip Y */
        Rectangle dst = { (float)draw_x, (float)draw_y, (float)draw_w, (float)draw_h };
        DrawTexturePro(target.texture, src, dst, (Vector2){0,0}, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(target);
    sd_sprites_unload();
    CloseWindow();
    return 0;
}
