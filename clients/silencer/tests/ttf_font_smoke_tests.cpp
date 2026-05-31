// SIL-11: SDL_ttf dependency + Silencer .otf font smoke test.
//
// Proves the four font faces (shared/fonts/silencer-{ui,ui-large,title,tiny}.otf,
// per SIL-6 decision 1) load and measure through SDL_ttf on this toolchain — the
// gating foundation for the golden FontRegistry + install_text_measurer (the TTF
// MeasureTextFn seam). No renderer/video needed: measurement is metrics-only and
// headless-safe.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdio.h>
#include <string.h>

#ifndef SILENCER_TEST_FONT_DIR
#define SILENCER_TEST_FONT_DIR "."
#endif

static bool measure_font(const char *name, float ptsize) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", SILENCER_TEST_FONT_DIR, name);
  TTF_Font *font = TTF_OpenFont(path, ptsize);
  if (!font) {
    fprintf(stderr, "FAIL: TTF_OpenFont(%s) -> %s\n", path, SDL_GetError());
    return false;
  }
  int w = 0, h = 0;
  bool ok = TTF_GetStringSize(font, "Silencer 0123", strlen("Silencer 0123"),
                              &w, &h);
  TTF_CloseFont(font);
  if (!ok || w <= 0 || h <= 0) {
    fprintf(stderr, "FAIL: measure %s -> ok=%d w=%d h=%d (%s)\n", name, ok, w, h,
            SDL_GetError());
    return false;
  }
  printf("OK: %s @ %.0fpt -> %dx%d\n", name, ptsize, w, h);
  return true;
}

int main(void) {
  if (!TTF_Init()) {
    fprintf(stderr, "FAIL: TTF_Init -> %s\n", SDL_GetError());
    return 1;
  }
  int fails = 0;
  // Integer-em-ish sizes per SIL-6 (11/13/24/5 family).
  if (!measure_font("silencer-ui.otf", 13.0f)) fails++;
  if (!measure_font("silencer-ui-large.otf", 13.0f)) fails++;
  if (!measure_font("silencer-title.otf", 24.0f)) fails++;
  if (!measure_font("silencer-tiny.otf", 5.0f)) fails++;
  TTF_Quit();
  return fails ? 1 : 0;
}
