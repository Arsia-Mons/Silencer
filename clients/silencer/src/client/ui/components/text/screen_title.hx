#pragma once

#include "ui/components/common.h"

#include <cstdint>

namespace silencer {

// Brand = compact RED "Silencer" wordmark; Console = small green lobby/console
// header; Prose = white heading (agency Advantages/Description over the backdrop).
enum class ScreenTitleVariant : uint8_t { Hero, Screen, Dialog, Popup, Brand, Console, Prose };

struct ScreenTitleProps {
  const char *key = nullptr;
  ScreenTitleVariant variant = ScreenTitleVariant::Screen;
  const char *value = "";
  bool centered = false; // center within the parent (origin titles sit in a notch)
};

::ui::UiElement ScreenTitle(const ScreenTitleProps &props);

} // namespace silencer
