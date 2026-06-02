#pragma once

// BooleanSettingRow: the origin/main boolean setting control (SIL-100) — an Oval
// Lg button carrying the label (click toggles) + a bank-6 two-cell sprite
// indicator (off = idx12|13 hollow, on = idx14|15 filled). Replaces the vector
// AppCheckbox on the options screens. Pulls its sprite ids from use_chrome().

#include "ui/components/common.h" // ::ui::UiElement

#include <functional>

namespace silencer {

struct BooleanSettingRowProps {
  const char *key = nullptr;
  const char *control_id = nullptr;
  bool checked = false;
  const char *label = nullptr;
  std::function<void(bool)> on_change = {};
};

::ui::UiElement BooleanSettingRow(const BooleanSettingRowProps &props);

} // namespace silencer
