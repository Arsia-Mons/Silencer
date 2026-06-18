// SIL-11: the cppx UI multi-face text-measure seam. Loads the four silencer
// faces, installs the TTF measurer into the SDL-free ui/ seam, and exercises
// measurement: single line, multi-face selection by font_id, parity with raw
// SDL_ttf, and word wrap. No renderer/video — measurement is metrics-only.

#include "render/cppx_ui/font_registry.h"
#include "render/cppx_ui/text_measure.h"
#include "ui/style/text_measure.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdio.h>
#include <string.h>

#ifndef SILENCER_TEST_FONT_DIR
#define SILENCER_TEST_FONT_DIR "."
#endif

static int g_fails = 0;
#define CHECK(cond, msg)                                                        \
  do {                                                                          \
    if (!(cond)) {                                                              \
      fprintf(stderr, "FAIL: %s\n", msg);                                       \
      ++g_fails;                                                                \
    }                                                                           \
  } while (0)

int main(void) {
  if (!TTF_Init()) {
    fprintf(stderr, "TTF_Init: %s\n", SDL_GetError());
    return 1;
  }

  silencer::cppx_ui::FontRegistry reg;
  CHECK(reg.load_faces(SILENCER_TEST_FONT_DIR), "load_faces");
  CHECK(reg.loaded(), "registry reports loaded");

  silencer::cppx_ui::install_text_measurer(&reg);
  ::ui::MeasureTextFn fn = ::ui::text_measurer();
  CHECK(fn != nullptr, "measurer installed into ui:: seam");

  if (fn) {
    const char *s = "Silencer 0123";
    const uint32_t slen = static_cast<uint32_t>(strlen(s));

    ::ui::TextMetricsQuery q = {};
    q.utf8 = s;
    q.len = slen;
    q.font_id = silencer::cppx_ui::FontRegistry::Body;
    q.font_size = 13;
    ::ui::TextMetricsResult r = fn(q);
    CHECK(r.width > 0.0f && r.height > 0.0f && r.line_count == 1,
          "body single-line measure");

    // Multi-face: the title face at the same size measures a different width.
    ::ui::TextMetricsQuery qt = q;
    qt.font_id = silencer::cppx_ui::FontRegistry::Title;
    ::ui::TextMetricsResult rt = fn(qt);
    CHECK(rt.width > 0.0f, "title measure");
    CHECK(rt.width != r.width, "font_id selects a distinct face");

    // Parity with raw SDL_ttf for the body face at the same size.
    TTF_Font *body = reg.face(silencer::cppx_ui::FontRegistry::Body);
    TTF_SetFontSize(body, 13.0f);
    int w = 0, h = 0;
    TTF_GetStringSize(body, s, slen, &w, &h);
    CHECK(static_cast<int>(r.width) == w, "measure matches TTF_GetStringSize");

    // Word wrap: a narrow box splits text into multiple lines.
    const char *lng = "alpha beta gamma delta epsilon";
    ::ui::TextMetricsQuery qw = {};
    qw.utf8 = lng;
    qw.len = static_cast<uint32_t>(strlen(lng));
    qw.font_id = silencer::cppx_ui::FontRegistry::Body;
    qw.font_size = 13;
    qw.wrap = ::ui::TextWrap::Words;
    qw.wrap_width = 50.0f;
    ::ui::TextMetricsResult rw = fn(qw);
    CHECK(rw.line_count >= 2, "word wrap produces multiple lines");

    printf("body=%.0fx%.0f title=%.0f ttf=%d wraplines=%d\n", r.width, r.height,
           rt.width, w, rw.line_count);
  }

  silencer::cppx_ui::install_text_measurer(nullptr);
  CHECK(::ui::text_measurer() == nullptr, "uninstall clears the seam");
  reg.shutdown();
  TTF_Quit();
  return g_fails ? 1 : 0;
}
