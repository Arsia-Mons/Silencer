#pragma once

#include "ui/state.h"

#include "ui/runtime/element.h"

namespace launcher {

// NEWS: the announcement list beside the selected announcement's blocks.
struct NewsTabProps {
  const char *key = nullptr;
  UiState *st = nullptr;
};

::ui::UiElement NewsTab(const NewsTabProps &props);

} // namespace launcher
