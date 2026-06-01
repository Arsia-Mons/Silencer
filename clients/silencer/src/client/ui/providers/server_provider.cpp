#include "server_provider.h"

#include "client/ui/hooks/use_server.h"
#include "ui/runtime/react.h"

namespace silencer::game_ui {

static ReactContext ServerContext = {};

::ui::UiElement ServerProvider(const ServerProviderValue &value,
                               ::ui::UiChildren children, const char *key) {
  const ServerProviderValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store server context\n");
    return ::ui::empty();
  }
  return ::ui::provider("ServerProvider", &ServerContext,
                        const_cast<ServerProviderValue *>(stored), children,
                        key);
}

Server use_server() {
  ServerProviderValue *value =
      static_cast<ServerProviderValue *>(use_context(&ServerContext));
  if (!value || !value->game) {
    react_report_error("client/ui: missing ServerProvider\n");
    return {};
  }
  return {
      .game = value->game,
  };
}

} // namespace silencer::game_ui
