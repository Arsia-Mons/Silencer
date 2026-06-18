#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "client/ui/hooks/use_session.h"
#include "ui/runtime/element.h"

namespace client::ui {

// The always-mounted stack root (entry 0): each frame it reads
// use_session().phase and renders the screen that owns that phase as its child.
// Always the only Normal screen on the stack, so the reconciler runs every
// frame regardless of overlays layered above.
class AppRoot final : public UiScreen {
public:
  const char *debug_name() const override { return "AppRoot"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

// Maps a session phase to the element that owns it.
::ui::UiElement make_phase_element(SessionPhase phase);

} // namespace client::ui
