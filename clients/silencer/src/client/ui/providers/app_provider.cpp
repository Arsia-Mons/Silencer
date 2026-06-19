#include "app_provider.h"

#include "client/ui/hooks/use_app.h"
#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext AppContext = {};

::ui::UiElement AppProvider(const AppProviderValue &value,
                            ::ui::UiChildren children, const char *key) {
  const AppProviderValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store app context\n");
    return ::ui::empty();
  }
  return ::ui::provider("AppProvider", &AppContext,
                        const_cast<AppProviderValue *>(stored), children, key);
}

App use_app() {
  AppProviderValue *value =
      static_cast<AppProviderValue *>(use_context(&AppContext));
  if (!value) {
    react_report_error("client/ui: missing AppProvider\n");
    return {};
  }
  return {
      .can_quit = static_cast<bool>(value->quit),
      .quit = value->quit,
      .version = value->version ? value->version : "",
      .canvas_w = value->canvas_w,
  };
}

} // namespace client::ui
