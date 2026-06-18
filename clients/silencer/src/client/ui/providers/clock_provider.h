#pragma once

#include "client/ui/hooks/use_clock.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Publishes the per-frame Clock into the global provider chain so
// components can read it via use_clock(). The composition root supplies the
// monotonic time; the provider holds only the POD value.
::ui::UiElement ClockProvider(const Clock &value, ::ui::UiChildren children,
                              const char *key = nullptr);

} // namespace client::ui
