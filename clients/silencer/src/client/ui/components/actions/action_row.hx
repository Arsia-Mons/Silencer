#pragma once

// ActionRow: horizontal row of actions (gap 12). Adapts ui::components::Box.
// Layout-only; carries no paint and no variant.

#include "ui/components/common.h" // ::ui::UiChildren, ::ui::UiElement

namespace silencer {

struct ActionRowProps {
  const char *key = nullptr;
  ::ui::UiChildren children = {};
};

::ui::UiElement ActionRow(const ActionRowProps &props);

} // namespace silencer
