#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Root of the Options cluster (doc §8). An Overlay pushed by MainMenu. Routes
// to the Audio/Display/Controls sub-screens via use_navigation (no GoToState),
// and owns the settings commit/revert: Done persists, Cancel reverts the live
// preview. Sub-screens apply changes live; this screen decides their fate.
class OptionsScreen final : public OverlayScreen {
public:
  const char *debug_name() const override { return "Options"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

} // namespace client::ui
