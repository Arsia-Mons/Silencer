#pragma once

#include "ui/state.h"

#include "ui/runtime/element.h"

namespace launcher {

// The main panel: the NEWS | RELEASES | SETTINGS tab strip over the selected
// tab's body.
struct ContentPanelProps {
  const char *key = nullptr;
  UiState *st = nullptr;
};

::ui::UiElement ContentPanel(const ContentPanelProps &props);

} // namespace launcher
