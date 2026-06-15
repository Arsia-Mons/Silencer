#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

namespace client::ui {

// The in-match pause affordance: an Overlay pushed over InGameScreen by its
// use_cancel chain (ESC/back with nothing else open) and by the projected
// mission-over edge (END_MISSION). It presents the origin "Hit Enter To Quit"
// prompt as an auto-focused primary action whose onPress leaves the match
// (use_session().leave_match → the origin CheckForQuit outcome). Dismiss is the
// cancel router's default overlay pop — PauseScreen registers no cancel handler.
// The Resume/Options/Leave menu is a SEPARATE later milestone.
class PauseScreen final : public OverlayScreen {
public:
  const char *debug_name() const override { return "Pause"; }
  // Origin behavior: the quit prompt cuts in over the live world with no fade.
  bool wants_transition_fade() const override { return false; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

} // namespace client::ui
