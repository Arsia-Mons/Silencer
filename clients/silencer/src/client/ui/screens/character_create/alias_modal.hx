#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

namespace client::ui {

// The "SILENCER ALIAS" entry overlay, pushed by the CharacterCreate roster's
// Create New Character button. It owns the typed alias (a use_state cell, so the
// field value is stable across the deferred Input render — no transient-pointer
// hazard). Enter advances to the agency overlay carrying the alias; Escape
// auto-pops back to the roster (the roster shows through, since the alias uses a
// transparent CenteredOverlay). Cuts in instantly (no transition fade).
class AliasModalScreen final : public OverlayScreen {
public:
  const char *debug_name() const override { return "AliasModal"; }
  bool wants_transition_fade() const override { return false; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

} // namespace client::ui
