// SIL-11: the DrawCommand IR -> pixels executor. Renders the RGBA IR through a
// headless software SDL_Renderer and reads back pixels: a solid Rect fill and a
// TTF Text glyph (exercising the multi-face font cache end-to-end).

#include "render/cppx_ui/draw_executor.h"
#include "render/cppx_ui/font_registry.h"
#include "ui/runtime/draw_command.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdio.h>
#include <stdlib.h>

#ifndef SILENCER_TEST_FONT_DIR
#define SILENCER_TEST_FONT_DIR "."
#endif

static int g_fails = 0;
#define CHECK(cond, msg)                                                        \
  do {                                                                          \
    if (!(cond)) {                                                              \
      fprintf(stderr, "FAIL: %s (%s)\n", msg, SDL_GetError());                  \
      ++g_fails;                                                                \
    }                                                                           \
  } while (0)

static void read_px(SDL_Surface *s, int x, int y, Uint8 out[4]) {
  const Uint8 *p =
      static_cast<const Uint8 *>(s->pixels) + y * s->pitch + x * 4;
  out[0] = p[0]; // SDL_PIXELFORMAT_RGBA32 is R,G,B,A in memory order
  out[1] = p[1];
  out[2] = p[2];
  out[3] = p[3];
}

static ui::DrawCommandList g_list; // ~256KB — keep off the stack

int main(void) {
  if (!SDL_Init(0)) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  TTF_Init();

  const int W = 32, H = 32;
  SDL_Surface *surf = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_RGBA32);
  SDL_Renderer *r = surf ? SDL_CreateSoftwareRenderer(surf) : nullptr;
  CHECK(surf && r, "software renderer");

  if (r) {
    // --- Scene 1: a solid red rect over opaque black. ---
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    g_list.reset();
    ui::DrawCommand rect{
        .kind = ui::DrawCommandKind::Rect,
        .rect = {8.f, 8.f, 16.f, 16.f},
        .payload = {.rect = {.fill = {255, 0, 0, 255}, .corner_radius = 0.f}}};
    CHECK(g_list.push(rect), "push rect");
    silencer::cppx_ui::execute_draw_commands(r, g_list, nullptr, nullptr);
    SDL_RenderPresent(r);

    Uint8 center[4], corner[4];
    read_px(surf, 16, 16, center);
    read_px(surf, 1, 1, corner);
    CHECK(center[0] > 200 && center[1] < 60 && center[2] < 60,
          "rect interior is red");
    CHECK(corner[0] < 60 && corner[1] < 60 && corner[2] < 60,
          "outside the rect is background black");

    // --- Scene 2: a TTF glyph (multi-face font cache). ---
    silencer::cppx_ui::FontRegistry fonts;
    CHECK(fonts.load_faces(SILENCER_TEST_FONT_DIR), "load faces");

    // Rasterization works at the FontRegistry layer.
    int tw = 0, th = 0;
    SDL_Texture *glyph = fonts.cached_text_texture(
        r, 0, "A", 1, 18, SDL_Color{255, 255, 255, 255}, &tw, &th);
    CHECK(glyph != nullptr && tw > 0 && th > 0, "cached_text_texture rasterizes");

    // Render over a distinct background so the glyph shows regardless of its
    // ink color (the .otf are color faces, rendered in their own ink).
    const Uint8 BG[3] = {17, 99, 213};
    SDL_SetRenderDrawColor(r, BG[0], BG[1], BG[2], 255);
    SDL_RenderClear(r);
    g_list.reset();
    uint32_t off = 0;
    CHECK(g_list.push_text("A", 1, &off), "push text bytes");
    ui::DrawCommand text{.kind = ui::DrawCommandKind::Text,
                         .rect = {6.f, 4.f, 20.f, 24.f},
                         .payload = {.text = {.text_off = off,
                                              .text_len = 1,
                                              .color = {255, 255, 255, 255},
                                              .font_id = 0,
                                              .font_size = 18}}};
    CHECK(g_list.push(text), "push text cmd");
    silencer::cppx_ui::execute_draw_commands(r, g_list, &fonts, nullptr);
    SDL_RenderPresent(r);

    // Count pixels that deviate from the background (the glyph drew them).
    int ink = 0, sr = -1, sg = 0, sb = 0;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        Uint8 p[4];
        read_px(surf, x, y, p);
        if (abs(p[0] - BG[0]) > 24 || abs(p[1] - BG[1]) > 24 ||
            abs(p[2] - BG[2]) > 24) {
          ++ink;
          if (sr < 0) { sr = p[0]; sg = p[1]; sb = p[2]; }
        }
      }
    fonts.shutdown();

    // The mono-glyf faces (SIL-6 fallback: 'SVG ' stripped) render visible ink
    // in the requested token color.
    CHECK(ink > 0, "glyph drew visible pixels (token color)");

    printf("draw executor ok: rect fill + TTF glyph render (tex %dx%d, %d ink "
           "px, ink~(%d,%d,%d))\n",
           tw, th, ink, sr, sg, sb);
  }

  if (r)
    SDL_DestroyRenderer(r);
  if (surf)
    SDL_DestroySurface(surf);
  TTF_Quit();
  SDL_Quit();
  return g_fails ? 1 : 0;
}
