#include "client/ui/app_shell/app_root.h"

#include "client/ui/hooks/use_session.h"
#include "client/ui/screens/character_create.h"
#include "client/ui/screens/lobby_connect.h"
#include "client/ui/screens/main_menu.h"
#include "client/ui/screens/mission_summary.h"
#include "client/ui/screens/update_screen.h"
#include "ui/runtime/element.h"
#include "ui/runtime/react.h"

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
  // Distinct keys per phase so a phase flip is a genuine unmount/mount (the
  // real screens are distinct component types, so this falls out naturally
  // once they replace the scaffolds).
  switch (phase) {
  case SessionPhase::MainMenu:
    return MainMenu("phase-main-menu");
  case SessionPhase::Connecting:
    return LobbyConnect("phase-connecting");
  case SessionPhase::CharacterCreate:
    return CharacterCreate("phase-character-create");
  case SessionPhase::Lobby:
    return phase_scaffold({36, 72, 60, 255}, "phase-lobby");
  case SessionPhase::Updating:
    return UpdateScreen("phase-updating");
  case SessionPhase::Loading:
    return phase_scaffold({56, 56, 56, 255}, "phase-loading");
  case SessionPhase::InMatch:
    return phase_scaffold({40, 96, 40, 255}, "phase-in-match");
  case SessionPhase::PostMatch:
    return MissionSummary("phase-post-match");
  case SessionPhase::SinglePlayer:
    return phase_scaffold({48, 80, 96, 255}, "phase-single-player");
  }
  return phase_scaffold({32, 44, 72, 255}, "phase-main-menu");
}

} // namespace client::ui
