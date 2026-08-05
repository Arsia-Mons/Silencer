#pragma once

#include "ui/state.h"

#include "ui/runtime/element.h"

namespace launcher {

// Sign-in popover, anchored under the topbar's account chip. Root overlay:
// absolutely positioned against the root box, not the chip.
struct AuthPopoverProps {
  const char *key = nullptr;
  UiState *st = nullptr;
};

::ui::UiElement AuthPopover(const AuthPopoverProps &props);

} // namespace launcher
