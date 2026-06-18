#include "settings_provider.h"

#include "client/ui/hooks/use_settings.h"
#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext SettingsContext = {};

::ui::UiElement SettingsProvider(const SettingsProviderValue &value,
                                 ::ui::UiChildren children, const char *key) {
  const SettingsProviderValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store settings context\n");
    return ::ui::empty();
  }
  return ::ui::provider("SettingsProvider", &SettingsContext,
                        const_cast<SettingsProviderValue *>(stored), children,
                        key);
}

Settings use_settings() {
  SettingsProviderValue *value =
      static_cast<SettingsProviderValue *>(use_context(&SettingsContext));
  if (!value) {
    react_report_error("client/ui: missing SettingsProvider\n");
    return {};
  }
  return value->settings;
}

} // namespace client::ui
