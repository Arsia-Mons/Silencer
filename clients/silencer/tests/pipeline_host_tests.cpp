// SIL-14: the production render seam — PipelineHost drives the real golden
// client::ui::UiPipeline and executes its tagged-union IR into pixels via the
// SIL-11 UiSurface/execute_draw_commands path. Headless: a stub screen (a
// viewport-filling fill box) is pushed, one frame is rendered, and the packed
// RGBA is checked for the fill. This is the bridge SIL-13 deferred — pipeline
// (SIL-13) -> renderer (SIL-11) — verified end-to-end without a window.

#include "render/cppx_ui/pipeline_host.h"

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "client/ui/app_shell/ui_pipeline.h"
#include "ui/runtime/element.h"
#include "ui/runtime/react.h"

#include <SDL3/SDL.h>

#include <memory>
#include <stdio.h>

#ifndef SILENCER_TEST_FONT_DIR
#define SILENCER_TEST_FONT_DIR "."
#endif

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

// A scaffold stub screen: a viewport-filling fill box of a given color. Real
// per-phase screens come with the AppRoot/session commit; this proves the
// pipeline->renderer bridge produces pixels.
struct StubProps {
  ::ui::Color color = {};
};

static ::ui::UiElement StubView(const StubProps &props) {
  ::ui::HostProps host = {};
  host.id = "StubFill";
  host.style.width = ::ui::Length::percent(100.0f);
  host.style.height = ::ui::Length::percent(100.0f);
  host.visual.background = props.color;
  return ::ui::box(host);
}

class StubScreen final : public client::ui::UiScreen {
public:
  explicit StubScreen(::ui::Color color) : color_(color) {}
  const char *debug_name() const override { return "Stub"; }
  bool build_element(::ui::UiElementFrame &, ::ui::UiElement *out) override {
    if (!out)
      return false;
    *out = ::ui::component("StubView", StubProps{.color = color_}, StubView,
                           "stub");
    return true;
  }
  void build_ui() override {}

private:
  ::ui::Color color_;
};

static client::ui::UiPipelineFrame test_frame(int w, int h) {
  client::ui::UiPipelineFrame frame = {};
  frame.layout = {(float)w, (float)h};
  frame.pointer = {-1000.0f, -1000.0f};
  return frame;
}

static bool pipeline_host_renders_pushed_screen_to_pixels() {
  react_init_runtime();
  silencer::cppx_ui::PipelineHost host;
  CHECK(host.ensure(64, 48, SILENCER_TEST_FONT_DIR));

  const ::ui::Color fill{200, 40, 160, 255}; // opaque magenta
  CHECK(host.pipeline().client_ui().push_screen(
      std::make_unique<StubScreen>(fill)));

  int w = 0, h = 0;
  const uint8_t *rgba = host.render(test_frame(64, 48), &w, &h);
  CHECK(rgba != nullptr);
  CHECK(w == 64 && h == 48);

  // Center pixel should carry the opaque fill (premultiplied; a==255 keeps RGB).
  const uint8_t *px = rgba + ((size_t)(h / 2) * w + (w / 2)) * 4u;
  CHECK(px[3] > 200);            // opaque
  CHECK(px[0] > 120 && px[0] > px[1]); // red-dominant over green
  CHECK(px[2] > 80);             // blue present (magenta, not pure red)
  return true;
}

static bool pipeline_host_empty_stack_is_transparent() {
  react_init_runtime();
  silencer::cppx_ui::PipelineHost host;
  CHECK(host.ensure(32, 32, SILENCER_TEST_FONT_DIR));
  int w = 0, h = 0;
  const uint8_t *rgba = host.render(test_frame(32, 32), &w, &h);
  CHECK(rgba != nullptr);
  // No screens -> nothing drawn -> fully transparent (alpha 0) center pixel.
  const uint8_t *px = rgba + ((size_t)(h / 2) * w + (w / 2)) * 4u;
  CHECK(px[3] == 0);
  return true;
}

int main(void) {
  if (!SDL_Init(0)) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  int rc = 0;
  if (!pipeline_host_renders_pushed_screen_to_pixels())
    rc = 1;
  if (!pipeline_host_empty_stack_is_transparent())
    rc = 1;
  react_shutdown();
  SDL_Quit();
  if (rc == 0)
    printf("pipeline_host_tests: OK\n");
  return rc;
}
