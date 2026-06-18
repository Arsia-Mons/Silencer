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

// SIL-17: the product-intent semantic component layer (namespace silencer::).
#include "client/ui/components/actions/actions.h"
#include "client/ui/components/layout/layout.h"
#include "client/ui/components/text/text.h"

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

  // MainMenu renders ScreenLayout(Menu) — with no chrome starfield baked in the
  // hermetic host, the opaque black menu surface (tokens::kSurfaceMenu).
  int w = 0, h = 0;
  const uint8_t *rgba = host.render(test_frame(64, 48), &w, &h);
  CHECK(rgba != nullptr);
  const uint8_t *px = rgba + ((size_t)(h / 2) * w + (w / 2)) * 4u;
  CHECK(px[3] > 200);                          // opaque
  CHECK(px[0] < 40 && px[1] < 40 && px[2] < 40); // black menu surface
  const uint8_t menu0 = px[0], menu1 = px[1], menu2 = px[2], menu3 = px[3];

  // Flip to InMatch -> AppRoot now renders InGameScreen (a transparent HUD that
  // overlays the live world) in place of the menu. The swap is reflected the
  // same frame: the center pixel is no longer the menu surface. (The HUD's exact
  // paint is intentionally not asserted — it floats over gameplay, so a stable
  // color check doesn't fit; what matters is the reconciler swapped the screen.)
  phase = client::ui::SessionPhase::InMatch;
  rgba = host.render(test_frame(64, 48), &w, &h);
  CHECK(rgba != nullptr);
  px = rgba + ((size_t)(h / 2) * w + (w / 2)) * 4u;
  CHECK(px[0] != menu0 || px[1] != menu1 || px[2] != menu2 || px[3] != menu3);
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
  // Options is a Tier-1 overlay pushed above MainMenu, not a game state, so it
  // never reaches the projection; the boot sentinel NONE falls back to MainMenu.
  CHECK(project_session_phase(GameState::NONE, 0) == P::MainMenu);
  // During a FADEOUT transition the OUTGOING screen stays mounted and fades to
  // black before the switch; the second arg is the state being faded FROM. Only
  // after the fade completes does `state` flip to the destination and the new
  // screen fade in, so the projection must hold the source here, not the target.
  CHECK(project_session_phase(GameState::FADEOUT, GameState::MAINMENU) == P::MainMenu);
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

// SIL-17: a screen built PURELY from the semantic component layer renders. The
// tree is silencer::ScreenLayout (Menu variant -> opaque slate menu surface)
// wrapping a silencer::ScreenTitle (Hero) + a silencer::AppButton (Primary ->
// accent-blue fill). No generic primitive, host, or VisualStyle appears in the
// authored screen: paint resolves entirely through silencer::tokens + the
// theme. A corner pixel carries the opaque menu surface; the center carries the
// accent-blue button fill.
struct SemanticScreenProps {};

static ::ui::UiElement SemanticScreenView(const SemanticScreenProps &) {
  silencer::ScreenLayoutProps layout = {};
  layout.variant = silencer::ScreenLayoutVariant::Menu;

  silencer::ScreenTitleProps title = {};
  title.variant = silencer::ScreenTitleVariant::Hero;
  title.value = "Silencer";

  silencer::AppButtonProps button = {};
  button.variant = silencer::AppButtonVariant::Primary;
  button.label = "Play";

  layout.children = ::ui::children({
      ::ui::component("ScreenTitle", title, silencer::ScreenTitle, "title"),
      ::ui::component("AppButton", button, silencer::AppButton, "play"),
  });
  return ::ui::component("ScreenLayout", layout, silencer::ScreenLayout,
                         "root");
}

class SemanticScreen final : public client::ui::UiScreen {
public:
  const char *debug_name() const override { return "Semantic"; }
  bool build_element(::ui::UiElementFrame &, ::ui::UiElement *out) override {
    if (!out)
      return false;
    *out = ::ui::component("SemanticScreenView", SemanticScreenProps{},
                           SemanticScreenView, "semantic");
    return true;
  }
  void build_ui() override {}
};

static bool pipeline_host_renders_semantic_screen() {
  react_init_runtime();
  silencer::cppx_ui::PipelineHost host;
  CHECK(host.ensure(64, 48, SILENCER_TEST_FONT_DIR));
  CHECK(host.pipeline().client_ui().push_screen(
      std::make_unique<SemanticScreen>()));

  int w = 0, h = 0;
  const uint8_t *rgba = host.render(test_frame(64, 48), &w, &h);
  CHECK(rgba != nullptr);
  CHECK(w == 64 && h == 48);

  auto pixel = [&](int x, int y) { return rgba + ((size_t)y * w + x) * 4u; };

  // The Menu ScreenLayout fills the viewport with the opaque dark slate menu
  // surface (tokens::kSurfaceMenu = {8,14,18,255}). Mid-column gap row 30 is
  // bare surface (title sits above, button below), so it shows the fill.
  const uint8_t *surface = pixel(0, 30);
  CHECK(surface[3] > 200);                                         // opaque
  CHECK(surface[0] < 40 && surface[1] < 40 && surface[2] < 40);    // dark slate

  // The ScreenTitle (Hero) paints green-phosphor glyphs in the top rows
  // (tokens::kTextHeroTitle = {48,168,44,255}). The hermetic host has no glyph
  // atlas baked, so sample across the title band for any green-dominant lit
  // pixel rather than pinning one glyph coordinate.
  bool title_lit = false;
  for (int y = 4; y < 20 && !title_lit; ++y)
    for (int x = 0; x < w && !title_lit; ++x) {
      const uint8_t *t = pixel(x, y);
      title_lit = t[3] > 200 && t[1] > 80 && t[1] > t[0] && t[1] > t[2];
    }
  CHECK(title_lit); // green-phosphor hero title rendered

  // The AppButton (Primary) paints its green accent fill across the bottom band
  // (tokens::kAccent = {92,208,92,255}); the 132x38 button overflows the 64px
  // width so it spans the row, green-dominant over red/blue.
  const uint8_t *button = pixel(w / 2, 40);
  CHECK(button[3] > 200);                          // opaque button fill
  CHECK(button[1] > button[0] && button[1] > button[2]); // green-dominant
  CHECK(button[1] > surface[1] + 40);              // markedly greener than surface
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
  if (!pipeline_host_renders_semantic_screen())
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
