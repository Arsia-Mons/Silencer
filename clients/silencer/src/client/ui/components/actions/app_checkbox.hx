#pragma once

// AppCheckbox: shared semantic checkbox. Adapts ui::components::Checkbox 1:1.
// Single appearance today (no variant); paint resolves from theme.checkbox.

#include "ui/components/common.h" // ::ui::UiElement

#include <functional>

namespace silencer {

struct AppCheckboxProps {
  const char *key = nullptr;
  const char *control_id = nullptr;
  bool checked = false;
  const char *label = nullptr;
  std::function<void(bool)> on_change = {};
};

::ui::UiElement AppCheckbox(const AppCheckboxProps &props);

} // namespace silencer
