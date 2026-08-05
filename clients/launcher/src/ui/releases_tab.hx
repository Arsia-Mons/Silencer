#pragma once

#include "ui/state.h"

#include "ui/runtime/element.h"

namespace launcher {

// RELEASES: the active channel's releases beside the selected one's notes.
struct ReleasesTabProps {
  const char *key = nullptr;
  UiState *st = nullptr;
};

::ui::UiElement ReleasesTab(const ReleasesTabProps &props);

} // namespace launcher
