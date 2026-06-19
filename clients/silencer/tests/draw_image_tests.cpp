// SIL-11: the executor's Image path — plain textured rect and the nine-slice
// (9-patch) button rendering, the "nine-slice button" half of the acceptance.
// Headless via a software SDL_Renderer + texture_registry upload.

#include "render/cppx_ui/draw_executor.h"
#include "render/cppx_ui/texture_registry.h"
#include "ui/runtime/draw_command.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

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
static void set_px(Uint8 *buf, int w, int x, int y, Uint8 r, Uint8 g, Uint8 b,
                   Uint8 a) {
  Uint8 *p = buf + (y * w + x) * 4;
  p[0] = r; p[1] = g; p[2] = b; p[3] = a;
}
static bool near(const Uint8 c[4], Uint8 r, Uint8 g, Uint8 b) {
  return abs(c[0] - r) < 40 && abs(c[1] - g) < 40 && abs(c[2] - b) < 40;
}

static ui::DrawCommandList g_list;

int main(void) {
  if (!SDL_Init(0)) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  const int W = 32, H = 32;
  SDL_Surface *surf = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_RGBA32);
  SDL_Renderer *r = surf ? SDL_CreateSoftwareRenderer(surf) : nullptr;
  CHECK(surf && r, "software renderer");

  if (r) {
    silencer::cppx_ui::TextureRegistry textures;

    // --- Scene 1: plain stretched image (solid magenta 2x2 -> 16x16). ---
    {
      uint8_t solid[2 * 2 * 4];
      for (int i = 0; i < 4; ++i)
        set_px(solid, 2, i % 2, i / 2, 255, 0, 255, 255);
      uint32_t id = textures.upload_rgba(r, solid, 2, 2);
      CHECK(id != 0, "upload solid texture");

      SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
      SDL_RenderClear(r);
      g_list.reset();
      ui::DrawCommand img{
          .kind = ui::DrawCommandKind::Image,
          .rect = {8.f, 8.f, 16.f, 16.f},
          .payload = {.image = {.texture_id = id,
                                .tint = {255, 255, 255, 255}}}};
      CHECK(g_list.push(img), "push plain image");
      silencer::cppx_ui::execute_draw_commands(r, g_list, nullptr, &textures);
      SDL_RenderPresent(r);
      Uint8 c[4];
      read_px(surf, 16, 16, c);
      CHECK(near(c, 255, 0, 255), "plain image fills with texture color");
    }

    // --- Scene 2: nine-slice 9-patch, corners land 1:1. ---
    {
      // 4x4: distinct opaque corners, white interior.
      uint8_t t[4 * 4 * 4];
      for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
          set_px(t, 4, x, y, 255, 255, 255, 255);
      set_px(t, 4, 0, 0, 255, 0, 0, 255);   // TL red
      set_px(t, 4, 3, 0, 0, 255, 0, 255);   // TR green
      set_px(t, 4, 0, 3, 0, 0, 255, 255);   // BL blue
      set_px(t, 4, 3, 3, 255, 255, 0, 255); // BR yellow
      uint32_t id = textures.upload_rgba(r, t, 4, 4);
      CHECK(id != 0, "upload 9-patch texture");

      SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
      SDL_RenderClear(r);
      g_list.reset();
      ui::DrawCommand img{
          .kind = ui::DrawCommandKind::Image,
          .rect = {0.f, 0.f, 16.f, 16.f},
          .payload = {.image = {.texture_id = id,
                                .tint = {255, 255, 255, 255},
                                .nine_slice = {1.f, 1.f, 1.f, 1.f}}}};
      CHECK(g_list.push(img), "push nine-slice image");
      silencer::cppx_ui::execute_draw_commands(r, g_list, nullptr, &textures);
      SDL_RenderPresent(r);

      // 1px caps => the four source corners map 1:1 onto the dest corners.
      Uint8 tl[4], tr[4], bl[4], br[4];
      read_px(surf, 0, 0, tl);
      read_px(surf, 15, 0, tr);
      read_px(surf, 0, 15, bl);
      read_px(surf, 15, 15, br);
      CHECK(near(tl, 255, 0, 0), "9-patch TL corner = red (1:1)");
      CHECK(near(tr, 0, 255, 0), "9-patch TR corner = green (1:1)");
      CHECK(near(bl, 0, 0, 255), "9-patch BL corner = blue (1:1)");
      CHECK(near(br, 255, 255, 0), "9-patch BR corner = yellow (1:1)");
    }

    // --- Scene 3: source sub-rect (atlasing / partial-fill, SIL-93). A 2x1
    // atlas — left texel red, right texel green — rendered selecting only the
    // RIGHT texel (src_x=1,w=1) must fill the whole dest with green, proving
    // the executor samples the sub-region, not the full texture. ---
    {
      uint8_t atlas[2 * 1 * 4];
      set_px(atlas, 2, 0, 0, 255, 0, 0, 255); // left = red
      set_px(atlas, 2, 1, 0, 0, 255, 0, 255); // right = green
      uint32_t id = textures.upload_rgba(r, atlas, 2, 1);
      CHECK(id != 0, "upload atlas texture");

      SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
      SDL_RenderClear(r);
      g_list.reset();
      ui::DrawCommand img{
          .kind = ui::DrawCommandKind::Image,
          .rect = {0.f, 0.f, 16.f, 16.f},
          .payload = {.image = {.texture_id = id,
                                .tint = {255, 255, 255, 255},
                                .src_x = 1.f,
                                .src_y = 0.f,
                                .src_w = 1.f,
                                .src_h = 1.f}}};
      CHECK(g_list.push(img), "push sub-rect image");
      silencer::cppx_ui::execute_draw_commands(r, g_list, nullptr, &textures);
      SDL_RenderPresent(r);
      Uint8 c[4];
      read_px(surf, 8, 8, c);
      CHECK(near(c, 0, 255, 0), "sub-rect samples the RIGHT (green) atlas cell");
    }

    if (g_fails == 0)
      printf("draw image ok: plain stretch + nine-slice corners 1:1 + "
             "sub-rect atlas sampling\n");
  }

  if (r)
    SDL_DestroyRenderer(r);
  if (surf)
    SDL_DestroySurface(surf);
  SDL_Quit();
  return g_fails ? 1 : 0;
}
