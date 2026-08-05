#pragma once

#include "ui/state.h"

#include "ui/runtime/element.h"

namespace launcher {

// Modal confirmation over a scrim: removing an installed channel from disk.
struct ConfirmOverlayProps {
  const char *key = nullptr;
  UiState *st = nullptr;
};

::ui::UiElement ConfirmOverlay(const ConfirmOverlayProps &props);

} // namespace launcher
