#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Controls sub-screen of the Options cluster (doc §8, §7b). An Overlay pushed by
// OptionsScreen. Renders the rows-of-combos read view from use_key_map and
// drives multi-device rebinds through use_keybind_capture (begin → device edges
// build the chord in the game layer → Confirm commits). Preset cycle + commit/
// revert + Back. (A scrollable full-action list is a follow-up — it needs a
// dynamic-children builder + a scroll primitive the runtime does not yet have;
// a representative set of actions is shown.)
class OptionsControlsScreen final : public OverlayScreen {
public:
  const char *debug_name() const override { return "OptionsControls"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

} // namespace client::ui
