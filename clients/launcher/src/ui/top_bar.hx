#pragma once

#include "ui/state.h"

#include "ui/runtime/element.h"

namespace launcher {

// The 48px chrome bar: logo, WEBSITE/DISCORD links, account chip.
struct TopBarProps {
  const char *key = nullptr;
  UiState *st = nullptr;
};

::ui::UiElement TopBar(const TopBarProps &props);

} // namespace launcher
