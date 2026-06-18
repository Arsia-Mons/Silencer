#pragma once

// AppInput: shared semantic text input. Adapts ui::components::Input 1:1.
// Single appearance today (no variant); paint resolves from theme.

#include "ui/components/common.h" // ::ui::UiElement

#include <functional>
#include <string>

namespace silencer {

struct AppInputProps {
  const char *key = nullptr;
  const char *control_id = nullptr;
  const char *accessibility_label = nullptr;
  const char *value = "";
  std::function<void(const std::string &)> on_change = {};
};

::ui::UiElement AppInput(const AppInputProps &props);

} // namespace silencer
