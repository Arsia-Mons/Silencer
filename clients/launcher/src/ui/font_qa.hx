#pragma once

#include "ui/runtime/element.h"

namespace launcher {

// Style-QA specimen: every glyph face at a ramp of sizes, labeled. Replaces the
// content panel under SILENCER_LAUNCHER_UI_STATE=fontqa.
struct FontQaPanelProps {
  const char *key = nullptr;
};

::ui::UiElement FontQaPanel(const FontQaPanelProps &props);

} // namespace launcher
