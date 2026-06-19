#pragma once

// BooleanSettingRow: an Oval Lg button label (click toggles) + a bank-6
// two-cell sprite indicator from use_chrome().

#include "ui/components/common.h" // ::ui::UiElement

#include <functional>

namespace silencer {

struct BooleanSettingRowProps {
  const char *key = nullptr;
  const char *control_id = nullptr;
  bool checked = false;
  const char *label = nullptr;
  // Margin-left on the indicator pair (logical px): keeps the toggle sprites on
  // their golden cells when the row is shifted for its label.
  float ind_dx = 0.0f;
  std::function<void(bool)> on_change = {};
};

::ui::UiElement BooleanSettingRow(const BooleanSettingRowProps &props);

} // namespace silencer
