// SIL-11: sprite->RGBA bake. Verifies an indexed sprite + palette flattens to
// premultiplied RGBA8 with index 0 transparent. Pure function, no renderer.

#include "render/cppx_ui/sprite_bake.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
#define CHECK(cond, msg)                                                        \
  do {                                                                          \
    if (!(cond)) {                                                              \
      fprintf(stderr, "FAIL: %s\n", msg);                                       \
      ++g_fails;                                                                \
    }                                                                           \
  } while (0)

int main(void) {
  SDL_Color pal[256];
  memset(pal, 0, sizeof(pal));
  pal[1] = SDL_Color{10, 20, 30, 255};
  pal[2] = SDL_Color{200, 100, 50, 255};
  pal[255] = SDL_Color{255, 255, 255, 255};

  // 2x2: [0, 1; 2, 255]
  const uint8_t idx[4] = {0, 1, 2, 255};
  uint8_t out[16];
  memset(out, 0xAB, sizeof(out)); // poison: confirm every byte is written
  silencer::cppx_ui::bake_indexed_rgba(idx, 2, 2, pal, out);

  CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 0,
        "index 0 -> transparent (0,0,0,0)");
  CHECK(out[4] == 10 && out[5] == 20 && out[6] == 30 && out[7] == 255,
        "index 1 -> (10,20,30,255) opaque");
  CHECK(out[8] == 200 && out[9] == 100 && out[10] == 50 && out[11] == 255,
        "index 2 -> (200,100,50,255)");
  CHECK(out[12] == 255 && out[13] == 255 && out[14] == 255 && out[15] == 255,
        "index 255 -> white opaque");

  // Premultiplied invariant: alpha 0 => rgb 0; alpha 255 => rgb unchanged.
  for (int p = 0; p < 4; ++p) {
    const uint8_t *o = out + p * 4;
    if (o[3] == 0)
      CHECK(o[0] == 0 && o[1] == 0 && o[2] == 0, "premultiplied: a=0 => rgb=0");
  }

  if (g_fails == 0)
    printf("sprite bake 2x2 ok: transparent index 0 + 3 opaque colors\n");
  return g_fails ? 1 : 0;
}
