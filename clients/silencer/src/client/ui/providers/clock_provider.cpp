#include "clock_provider.h"

#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext ClockContext = {};

::ui::UiElement ClockProvider(const Clock &value, ::ui::UiChildren children,
                              const char *key) {
  const Clock *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store clock context\n");
    return ::ui::empty();
  }
  return ::ui::provider("ClockProvider", &ClockContext,
                        const_cast<Clock *>(stored), children, key);
}

Clock use_clock() {
  Clock *value = static_cast<Clock *>(use_context(&ClockContext));
  // No provider mounted (e.g. an isolated test/gallery host) => zero clock, which
  // animating components read as "hold frame 0". Not an error.
  return value ? *value : Clock{};
}

} // namespace client::ui
