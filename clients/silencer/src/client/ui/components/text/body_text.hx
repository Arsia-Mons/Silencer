#pragma once

#include "ui/components/common.h"

#include <cstdint>

namespace silencer {

enum class BodyTextVariant : uint8_t { Body, Strong, Message, Detail, Summary };
enum class BodyTextTone : uint8_t { Default, Muted, Disabled };

struct BodyTextProps {
  const char *key = nullptr;
  BodyTextVariant variant = BodyTextVariant::Body;
  BodyTextTone tone = BodyTextTone::Default;
  const char *value = "";
};

::ui::UiElement BodyText(const BodyTextProps &props);

} // namespace silencer
