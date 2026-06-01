#include "session_provider.h"

#include "client/ui/hooks/use_session.h"
#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext SessionContext = {};

::ui::UiElement SessionProvider(const SessionProviderValue &value,
                                ::ui::UiChildren children, const char *key) {
  const SessionProviderValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store session context\n");
    return ::ui::empty();
  }
  return ::ui::provider("SessionProvider", &SessionContext,
                        const_cast<SessionProviderValue *>(stored), children,
                        key);
}

Session use_session() {
  SessionProviderValue *value =
      static_cast<SessionProviderValue *>(use_context(&SessionContext));
  if (!value) {
    react_report_error("client/ui: missing SessionProvider\n");
    return {};
  }
  return {
      .phase = value->phase,
  };
}

} // namespace client::ui
