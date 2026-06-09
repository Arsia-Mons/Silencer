#include "lobby_provider.h"

#include "client/ui/hooks/use_characters.h"
#include "client/ui/hooks/use_games.h"
#include "client/ui/hooks/use_lobby_chat.h"
#include "client/ui/hooks/use_lobby_session.h"
#include "client/ui/hooks/use_progression.h"
#include "client/ui/hooks/use_staging.h"
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
      .selected_summary = value->snapshot.lobby_agent,
      .create = value->create_character,
      .select = value->select_character,
  };
}

LobbyChat use_lobby_chat() {
  LobbyProviderValue *value =
      static_cast<LobbyProviderValue *>(use_context(&LobbyContext));
  if (!value) {
    react_report_error("client/ui: missing LobbyProvider\n");
    return {};
  }
  return {
      .scrollback = value->snapshot.lobby_chat,
      .presence = value->snapshot.lobby_presence,
      .channel = value->snapshot.chat_channel,
      .send = value->send_chat,
  };
}

Games use_games() {
  LobbyProviderValue *value =
      static_cast<LobbyProviderValue *>(use_context(&LobbyContext));
  if (!value) {
    react_report_error("client/ui: missing LobbyProvider\n");
    return {};
  }
  // can_join reads the per-tick snapshot's joinable flag (the same data the
  // entries carry) so the screen can gate by id without re-deriving rules.
  return {
      .entries = value->snapshot.games,
      .bundled_maps = value->snapshot.bundled_maps,
      .join_game = value->join_game,
      .spectate_game = value->spectate_game,
      .create_game = value->create_game,
      .can_join = [entries = value->snapshot.games](uint32_t id) {
        for (const GameBrowserEntry &e : entries)
          if (e.id == id)
            return e.joinable;
        return false;
      },
  };
}

Staging use_staging() {
  LobbyProviderValue *value =
      static_cast<LobbyProviderValue *>(use_context(&LobbyContext));
  if (!value) {
    react_report_error("client/ui: missing LobbyProvider\n");
    return {};
  }
  const LobbySnapshot &s = value->snapshot;
  return {
      .active = s.staging_active,
      .in_game_lobby = s.staging_in_lobby,
      .is_host = s.staging_is_host,
      .ready_blocked = s.staging_ready_blocked,
      .ready_label = s.staging_ready_label,
      .map_name = s.staging_map_name,
      .roster = s.staging_roster,
      .send_ready = value->send_ready,
      .change_team = value->change_team,
      .leave = value->leave_game,
  };
}

} // namespace client::ui
