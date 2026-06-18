#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

#include <string>
#include <utility>

namespace client::ui {

// The "SELECT AGENCY" view, pushed when an alias is confirmed. It is a full
// opaque screen (covers the roster, as origin's agency select does); it is a
// ScreenStack overlay entry only so that Escape is a single-press pop back to
// the roster. It carries the chosen alias and owns its own preview/submitting
// state. Picking an agency creates the agent (CREATECHARACTER tick → LOBBY) and
// pops back to the roster. Cuts in instantly (no transition fade).
class AgencyScreen final : public OverlayScreen {
public:
  explicit AgencyScreen(std::string alias) : alias_(std::move(alias)) {}

  const char *debug_name() const override { return "AgencyScreen"; }
  bool wants_transition_fade() const override { return false; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}

private:
  std::string alias_;
};

} // namespace client::ui
