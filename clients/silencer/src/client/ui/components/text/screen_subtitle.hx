#pragma once

#include "ui/components/common.h"

namespace silencer {

struct ScreenSubtitleProps {
  const char *key = nullptr;
  const char *value = "";
};

::ui::UiElement ScreenSubtitle(const ScreenSubtitleProps &props);

} // namespace silencer
