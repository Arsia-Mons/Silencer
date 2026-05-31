// SIL-11: the renderer-side texture_id -> SDL_Texture registry. Exercised
// headlessly via a software SDL_Renderer (offscreen surface, no video):
// upload_rgba/adopt/lookup, id allocation, invalid inputs, and the end-to-end
// sprite->bake->upload path.

#include "render/cppx_ui/sprite_bake.h"
#include "render/cppx_ui/texture_registry.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
#define CHECK(cond, msg)                                                        \
  do {                                                                          \
    if (!(cond)) {                                                              \
      fprintf(stderr, "FAIL: %s (%s)\n", msg, SDL_GetError());                  \
      ++g_fails;                                                                \
    }                                                                           \
  } while (0)

int main(void) {
  if (!SDL_Init(0)) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Surface *surf = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA32);
  CHECK(surf != nullptr, "create offscreen surface");
  SDL_Renderer *r = surf ? SDL_CreateSoftwareRenderer(surf) : nullptr;
  CHECK(r != nullptr, "create software renderer");

  if (r) {
    silencer::cppx_ui::TextureRegistry reg;
    CHECK(reg.lookup(0) == nullptr, "lookup(0) -> null (reserved)");

    const uint8_t rgba[16] = {255, 0, 0, 255, 0, 255, 0, 128,
                              0, 0, 255, 255, 255, 255, 255, 0};
    uint32_t id = reg.upload_rgba(r, rgba, 2, 2);
    CHECK(id == 1, "first upload -> id 1");
    CHECK(reg.lookup(id) != nullptr, "lookup(id) -> texture");
    CHECK(reg.lookup(999) == nullptr, "lookup(unknown) -> null");

    uint32_t id2 = reg.upload_rgba(r, rgba, 2, 2);
    CHECK(id2 == 2, "second upload -> id 2");

    CHECK(reg.upload_rgba(nullptr, rgba, 2, 2) == 0, "null renderer -> 0");
    CHECK(reg.upload_rgba(r, rgba, 0, 2) == 0, "zero width -> 0");

    // End-to-end: bake an indexed sprite, then upload it.
    SDL_Color pal[256];
    memset(pal, 0, sizeof(pal));
    pal[1] = SDL_Color{200, 50, 50, 255};
    const uint8_t idx[4] = {0, 1, 1, 0};
    uint8_t baked[16];
    silencer::cppx_ui::bake_indexed_rgba(idx, 2, 2, pal, baked);
    uint32_t bid = reg.upload_rgba(r, baked, 2, 2);
    CHECK(bid == 3, "bake -> upload -> id 3");
    CHECK(reg.lookup(bid) != nullptr, "baked texture present");

    reg.shutdown();
    CHECK(reg.lookup(1) == nullptr, "after shutdown all ids invalid");

    if (g_fails == 0)
      printf("texture registry ok: upload/adopt/lookup + bake->upload\n");
  }

  if (r)
    SDL_DestroyRenderer(r);
  if (surf)
    SDL_DestroySurface(surf);
  SDL_Quit();
  return g_fails ? 1 : 0;
}
