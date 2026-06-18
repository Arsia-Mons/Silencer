#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Display sub-screen of the Options cluster (doc §8). An Overlay pushed by
// OptionsScreen. Reads use_settings and live-applies fullscreen + smooth
// scaling (set_* preview immediately); Back pops without committing.
class OptionsDisplayScreen final : public OverlayScreen {
public:
  const char *debug_name() const override { return "OptionsDisplay"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

} // namespace client::ui
