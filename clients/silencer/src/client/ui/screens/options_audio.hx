#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Audio sub-screen of the Options cluster (doc §8). An Overlay pushed by
// OptionsScreen. Reads use_settings and live-applies music on/off + volume
// (set_* preview immediately); Back pops without committing (the Options root
// owns commit/revert).
class OptionsAudioScreen final : public OverlayScreen {
public:
  const char *debug_name() const override { return "OptionsAudio"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

} // namespace client::ui
