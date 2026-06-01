#pragma once

#include "ui/components/common.h"

#include <cstdint>

namespace silencer {

enum class ScreenTitleVariant : uint8_t { Hero, Screen, Dialog, Popup };

struct ScreenTitleProps {
  const char *key = nullptr;
  ScreenTitleVariant variant = ScreenTitleVariant::Screen;
  const char *value = "";
};

::ui::UiElement ScreenTitle(const ScreenTitleProps &props);

} // namespace silencer
