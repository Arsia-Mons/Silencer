#include "lobby_provider.h"

#include "client/ui/hooks/use_characters.h"
#include "client/ui/hooks/use_lobby_session.h"
#include "client/ui/hooks/use_progression.h"
#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext LobbyContext = {};

::ui::UiElement LobbyProvider(const LobbyProviderValue &value,
                              ::ui::UiChildren children, const char *key) {
  const LobbyProviderValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store lobby context\n");
    return ::ui::empty();
  }
  return ::ui::provider("LobbyProvider", &LobbyContext,
                        const_cast<LobbyProviderValue *>(stored), children, key);
}

LobbySession use_lobby_session() {
  LobbyProviderValue *value =
      static_cast<LobbyProviderValue *>(use_context(&LobbyContext));
  if (!value) {
    react_report_error("client/ui: missing LobbyProvider\n");
    return {};
  }
  const LobbySnapshot &s = value->snapshot;
  return {
      .status_log = s.status_log,
      .connecting = s.connecting,
      .awaiting_credentials = s.awaiting_credentials,
      .credentials_pending = s.credentials_pending,
      .authenticated = s.authenticated,
      .connect = value->connect,
      .cancel = value->cancel,
  };
}

Progression use_progression() {
  LobbyProviderValue *value =
      static_cast<LobbyProviderValue *>(use_context(&LobbyContext));
  if (!value) {
    react_report_error("client/ui: missing LobbyProvider\n");
    return {};
  }
  const LobbySnapshot &s = value->snapshot;
  Progression out;
  out.loaded = s.progression_loaded;
  out.experience = s.experience;
  out.stats_text = s.stats_text;
  out.upgrade_banner = s.upgrade_banner;
  for (int i = 0; i < 6; ++i) {
    out.levels[i] = s.levels[i];
    out.upgrades_available[i] = s.upgrades_available[i];
  }
  out.upgrade = value->upgrade;
  out.finish = value->finish;
  return out;
}

Characters use_characters() {
  LobbyProviderValue *value =
      static_cast<LobbyProviderValue *>(use_context(&LobbyContext));
  if (!value) {
    react_report_error("client/ui: missing LobbyProvider\n");
    return {};
  }
  return {
      .roster = value->snapshot.character_names,
      .received = value->snapshot.characters_received,
      .create = value->create_character,
      .select = value->select_character,
  };
}

} // namespace client::ui
