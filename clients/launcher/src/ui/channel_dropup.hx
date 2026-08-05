#pragma once

#include "ui/state.h"

#include "ui/runtime/element.h"

namespace launcher {

// Channel drop-up, anchored above the playbar's split button: one row per
// channel (select, install, update, uninstall) plus the install location.
struct ChannelDropupProps {
  const char *key = nullptr;
  UiState *st = nullptr;
};

::ui::UiElement ChannelDropup(const ChannelDropupProps &props);

} // namespace launcher
