// SIL-11: the per-frame UiSurface (clear -> execute IR -> resolve -> present),
// driving the draw_executor. Headless via a software SDL_Renderer. Covers the
// direct path and the supersample (full-scene SSAA) path.

#include "render/cppx_ui/draw_executor.h"
#include "render/cppx_ui/font_registry.h"
#include "render/cppx_ui/ui_surface.h"
#include "ui/runtime/draw_command.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdio.h>

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
  const Uint8 *p = static_cast<const Uint8 *>(s->pixels) + y * s->pitch + x * 4;
  out[0] = p[0];
  out[1] = p[1];
  out[2] = p[2];
  out[3] = p[3];
}

static ui::DrawCommandList g_list;
static void push_red_rect() {
  g_list.reset();
  ui::DrawCommand rect{
      .kind = ui::DrawCommandKind::Rect,
      .rect = {8.f, 8.f, 16.f, 16.f},
      .payload = {.rect = {.fill = {255, 0, 0, 255}, .corner_radius = 0.f}}};
  g_list.push(rect);
}

// Render one frame at `supersample` and assert the rect interior is red.
static void frame_renders_red(silencer::cppx_ui::UiSurface &ui, SDL_Surface *surf,
                              int supersample, const char *tag) {
  float scale =
      ui.begin_frame(ui::Color{0, 0, 0, 255}, 1.0f, supersample);
  CHECK(scale == static_cast<float>(supersample > 1 ? supersample : 1),
        "begin_frame returns expected scale");
  push_red_rect();
  silencer::cppx_ui::execute_draw_commands(ui.sdl_renderer(), g_list,
                                           ui.fonts(), nullptr, scale, {});
  ui.resolve_frame();
  ui.present();
  Uint8 c[4];
  read_px(surf, 16, 16, c);
  CHECK(c[0] > 180 && c[1] < 70 && c[2] < 70, tag);
}

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
    silencer::cppx_ui::FontRegistry fonts;
    CHECK(fonts.load_faces(SILENCER_TEST_FONT_DIR), "load faces");

    silencer::cppx_ui::UiSurface ui;
    CHECK(ui.initialize(r, fonts), "UiSurface initialize");
    CHECK(ui.sdl_renderer() == r && ui.fonts() == &fonts, "accessors");
    CHECK(ui.sdf_cache() != nullptr, "sdf cache present");

    frame_renders_red(ui, surf, 1, "direct path: rect interior red");
    frame_renders_red(ui, surf, 2, "supersample x2: rect interior red");

    ui.shutdown();
    if (g_fails == 0)
      printf("ui surface ok: direct + supersample frames drive the executor\n");
  }

  if (r)
    SDL_DestroyRenderer(r);
  if (surf)
    SDL_DestroySurface(surf);
  TTF_Quit();
  SDL_Quit();
  return g_fails ? 1 : 0;
}
