// SIL-14: the production render seam — PipelineHost drives the real golden
// client::ui::UiPipeline and executes its tagged-union IR into pixels via the
// SIL-11 UiSurface/execute_draw_commands path. Headless: a stub screen (a
// viewport-filling fill box) is pushed, one frame is rendered, and the packed
// RGBA is checked for the fill. This is the bridge SIL-13 deferred — pipeline
// (SIL-13) -> renderer (SIL-11) — verified end-to-end without a window.

#include "render/cppx_ui/pipeline_host.h"

#include "client/ui/app_shell/app_root.h"
#include "client/ui/app_shell/navigation/ui_screen.h"
#include "client/ui/app_shell/ui_pipeline.h"
#include "client/ui/hooks/use_session.h"
#include "client/ui/providers/session_provider.h"
#include "game/ui/session_phase.h"
#include "ui/components/components.h"
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

// SIL-14: AppRoot (the always-mounted stack root) reads use_session().phase and
// renders the owning phase's view as a child — the Tier-2 reconciler, expressed
// declaratively. Flipping the SessionProvider's phase swaps the rendered screen
// on the very next frame, with no stack churn.
static bool pipeline_host_app_root_reconciles_phase() {
  react_init_runtime();
  silencer::cppx_ui::PipelineHost host;
  CHECK(host.ensure(64, 48, SILENCER_TEST_FONT_DIR));

  client::ui::SessionPhase phase = client::ui::SessionPhase::MainMenu;
  host.pipeline().set_frame_provider([&phase](::ui::UiElement child) {
    client::ui::Session session = {};
    session.phase = phase;
    return client::ui::SessionProvider(client::ui::SessionProviderValue{session},
                                       ::ui::children({child}));
  });
  CHECK(host.pipeline().client_ui().push_screen(
      std::make_unique<client::ui::AppRoot>()));

  // MainMenu scaffold is {32,44,72} — blue-dominant.
  int w = 0, h = 0;
  const uint8_t *rgba = host.render(test_frame(64, 48), &w, &h);
  CHECK(rgba != nullptr);
  const uint8_t *px = rgba + ((size_t)(h / 2) * w + (w / 2)) * 4u;
  CHECK(px[3] > 200);                    // opaque
  CHECK(px[2] > px[0] && px[2] > px[1]); // blue-dominant

  // Flip to InMatch ({40,96,40}, green-dominant) — reflected the same frame.
  phase = client::ui::SessionPhase::InMatch;
  rgba = host.render(test_frame(64, 48), &w, &h);
  CHECK(rgba != nullptr);
  px = rgba + ((size_t)(h / 2) * w + (w / 2)) * 4u;
  CHECK(px[3] > 200);
  CHECK(px[1] > px[0] && px[1] > px[2]); // green-dominant
  return true;
}

// SIL-14: the live composition root projects the game's state machine onto the
// session phase the SessionProvider feeds AppRoot. Pure mapping, so verify it
// directly (the non-1:1 cases are the ones worth pinning).
static bool session_phase_projection_maps_game_states() {
  using P = client::ui::SessionPhase;
  using silencer::game_ui::project_session_phase;
  CHECK(project_session_phase(GameState::MAINMENU, 0) == P::MainMenu);
  CHECK(project_session_phase(GameState::LOBBYCONNECT, 0) == P::Connecting);
  CHECK(project_session_phase(GameState::LOBBY, 0) == P::Lobby);
  CHECK(project_session_phase(GameState::UPDATING, 0) == P::Updating);
  CHECK(project_session_phase(GameState::INGAME, 0) == P::InMatch);
  CHECK(project_session_phase(GameState::TESTGAME, 0) == P::InMatch);
  CHECK(project_session_phase(GameState::MISSIONSUMMARY, 0) == P::PostMatch);
  CHECK(project_session_phase(GameState::SINGLEPLAYERGAME, 0) == P::SinglePlayer);
  CHECK(project_session_phase(GameState::CREATECHARACTER, 0) == P::CharacterCreate);
  CHECK(project_session_phase(GameState::JOINGAME, 0) == P::Loading);
  CHECK(project_session_phase(GameState::HOSTGAME, 0) == P::Loading);
  // OPTIONS* are overlays over the menu in the retained model -> MainMenu under.
  CHECK(project_session_phase(GameState::OPTIONS, 0) == P::MainMenu);
  CHECK(project_session_phase(GameState::OPTIONSCONTROLS, 0) == P::MainMenu);
  CHECK(project_session_phase(GameState::NONE, 0) == P::MainMenu);
  // FADEOUT tracks the pending destination, not a fade limbo.
  CHECK(project_session_phase(GameState::FADEOUT, GameState::INGAME) == P::InMatch);
  CHECK(project_session_phase(GameState::FADEOUT, GameState::LOBBY) == P::Lobby);
  return true;
}

// SIL-16: the generic ui::components primitives render through the real
// pipeline. A Button bottoms out in detail::Host, resolves its paint from the
// (fallback) theme.button, and emits the draw IR the executor rasterizes — so a
// pixel inside its box carries the theme's opaque control fill.
struct PrimitiveScreenProps {};

static ::ui::UiElement PrimitiveScreenView(const PrimitiveScreenProps &) {
  ::ui::components::ButtonProps props = {};
  props.label = "OK";
  return ::ui::components::elements::Button(props);
}

class PrimitiveScreen final : public client::ui::UiScreen {
public:
  const char *debug_name() const override { return "Primitive"; }
  bool build_element(::ui::UiElementFrame &, ::ui::UiElement *out) override {
    if (!out)
      return false;
    *out = ::ui::component("PrimitiveScreenView", PrimitiveScreenProps{},
                           PrimitiveScreenView, "primitive");
    return true;
  }
  void build_ui() override {}
};

static bool pipeline_host_renders_generic_primitive() {
  react_init_runtime();
  silencer::cppx_ui::PipelineHost host;
  CHECK(host.ensure(64, 48, SILENCER_TEST_FONT_DIR));
  CHECK(host.pipeline().client_ui().push_screen(
      std::make_unique<PrimitiveScreen>()));

  int w = 0, h = 0;
  const uint8_t *rgba = host.render(test_frame(64, 48), &w, &h);
  CHECK(rgba != nullptr);
  // The Button (default theme.button control fill) sits at the origin and its
  // 132x38 box covers the top-left; a pixel inside it is an opaque neutral gray.
  const uint8_t *px = rgba + ((size_t)8 * w + 8) * 4u;
  CHECK(px[3] > 200);                                 // opaque control fill
  CHECK(px[0] > 24 && px[0] < 110);                   // neutral gray, not black/white
  CHECK(px[1] > 24 && px[1] < 110 && px[2] > 24 && px[2] < 120);
  return true;
}

int main(void) {
  if (!SDL_Init(0)) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  int rc = 0;
  if (!session_phase_projection_maps_game_states())
    rc = 1;
  if (!pipeline_host_renders_generic_primitive())
    rc = 1;
  if (!pipeline_host_renders_pushed_screen_to_pixels())
    rc = 1;
  if (!pipeline_host_empty_stack_is_transparent())
    rc = 1;
  if (!pipeline_host_app_root_reconciles_phase())
    rc = 1;
  react_shutdown();
  SDL_Quit();
  if (rc == 0)
    printf("pipeline_host_tests: OK\n");
  return rc;
}
