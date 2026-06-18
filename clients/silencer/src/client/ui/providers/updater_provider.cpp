#include "updater_provider.h"

#include "client/ui/hooks/use_updater.h"
#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext UpdaterContext = {};

::ui::UiElement UpdaterProvider(const UpdaterProviderValue &value,
                                ::ui::UiChildren children, const char *key) {
  const UpdaterProviderValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store updater context\n");
    return ::ui::empty();
  }
  return ::ui::provider("UpdaterProvider", &UpdaterContext,
                        const_cast<UpdaterProviderValue *>(stored), children,
                        key);
}

UpdaterModel use_updater() {
  UpdaterProviderValue *value =
      static_cast<UpdaterProviderValue *>(use_context(&UpdaterContext));
  if (!value) {
    react_report_error("client/ui: missing UpdaterProvider\n");
    return {};
  }
  return value->updater;
}

} // namespace client::ui
