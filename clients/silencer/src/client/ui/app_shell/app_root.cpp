#include "client/ui/app_shell/app_root.h"

#include "client/ui/hooks/use_session.h"
#include "client/ui/screens/character_create/character_create.h"
#include "client/ui/screens/in_game/in_game_screen.h"
#include "client/ui/screens/lobby_connect.h"
#include "client/ui/screens/lobby/lobby_screen.h"
#include "client/ui/screens/main_menu.h"
#include "client/ui/screens/mission_summary.h"
#include "client/ui/screens/update_screen.h"
#include "ui/runtime/element.h"
#include "ui/runtime/react.h"

#include <cstddef>
#include <iterator>

namespace client::ui {
namespace {

// --- Per-phase scaffold view (migration placeholder) ---------------------
// A full-viewport fill tinted per phase, so the phase reconciliation is
// observable on-screen and in tests for the phases whose real screens have not
// landed yet. SIL-19..21 replace the remaining scaffold cases with the authored
// screen views; AppRoot and the session projection stay put.
struct PhaseScaffoldProps {
  ::ui::Color color = {};
};

::ui::UiElement PhaseScaffoldView(const PhaseScaffoldProps &props) {
  ::ui::HostProps host = {};
  host.id = "PhaseScaffold";
  host.style.width = ::ui::Length::percent(100.0f);
  host.style.height = ::ui::Length::percent(100.0f);
  host.visual.background = props.color;
  return ::ui::box(host);
}

::ui::UiElement phase_scaffold(::ui::Color color, const char *key) {
  return ::ui::component("PhaseScaffoldView", PhaseScaffoldProps{.color = color},
                         PhaseScaffoldView, key);
}

::ui::UiElement loading_scaffold(const char *key) {
  return phase_scaffold({56, 56, 56, 255}, key);
}

// Phase -> screen-factory map, indexed by SessionPhase. Distinct keys per
// phase so a phase flip is a genuine unmount/mount (the real screens are
// distinct component types, so this falls out naturally).
struct PhaseScreen {
  ::ui::UiElement (*make)(const char *key);
  const char *key;
};
constexpr PhaseScreen kPhaseScreens[] = {
    /* MainMenu */ {MainMenu, "phase-main-menu"},
    /* Connecting */ {LobbyConnect, "phase-connecting"},
    /* CharacterCreate */ {CharacterCreate, "phase-character-create"},
    /* Lobby */ {LobbyScreen, "phase-lobby"},
    /* Updating */ {UpdateScreen, "phase-updating"},
    /* Loading */ {loading_scaffold, "phase-loading"},
    /* InMatch */ {InGameScreen, "phase-in-match"},
    /* PostMatch */ {MissionSummary, "phase-post-match"},
    /* SinglePlayer */ {InGameScreen, "phase-single-player"},
};

// --- AppRoot view --------------------------------------------------------
struct AppRootProps {};

::ui::UiElement AppRootView(const AppRootProps &) {
  Session session = use_session();
  return make_phase_element(session.phase);
}

} // namespace

bool AppRoot::build_element(::ui::UiElementFrame &, ::ui::UiElement *out) {
  if (!out)
    return false;
  *out = ::ui::component("AppRoot", AppRootProps{}, AppRootView, "app-root");
  return true;
}

::ui::UiElement make_phase_element(SessionPhase phase) {
  const size_t i = static_cast<size_t>(phase);
  if (i >= std::size(kPhaseScreens))
    return phase_scaffold({32, 44, 72, 255}, "phase-main-menu");
  return kPhaseScreens[i].make(kPhaseScreens[i].key);
}

} // namespace client::ui
