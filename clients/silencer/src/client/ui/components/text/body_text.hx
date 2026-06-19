#pragma once

#include "ui/components/common.h"

#include <cstdint>

namespace silencer {

enum class BodyTextVariant : uint8_t { Body, Strong, Message, Detail, Summary };
// Warning = amber (build version); Prose = white (agency detail/description).
// origin/main body text isn't all green.
enum class BodyTextTone : uint8_t { Default, Muted, Disabled, Warning, Prose };

struct BodyTextProps {
  const char *key = nullptr;
  BodyTextVariant variant = BodyTextVariant::Body;
  BodyTextTone tone = BodyTextTone::Default;
  const char *value = "";
};

::ui::UiElement BodyText(const BodyTextProps &props);

} // namespace silencer
