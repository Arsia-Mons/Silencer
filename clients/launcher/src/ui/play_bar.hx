#pragma once

#include "ui/state.h"

#include "ui/runtime/element.h"

namespace launcher {

// The bottom bar: lobby ping, install progress, and the split
// channel/INSTALL-PLAY button.
struct PlayBarProps {
  const char *key = nullptr;
  UiState *st = nullptr;
};

::ui::UiElement PlayBar(const PlayBarProps &props);

} // namespace launcher
