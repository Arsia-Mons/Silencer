#pragma once

#include "client/ui/hooks/use_clock.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Publishes the per-frame Clock for use_clock().
::ui::UiElement ClockProvider(const Clock &value, ::ui::UiChildren children,
                              const char *key = nullptr);

} // namespace client::ui
