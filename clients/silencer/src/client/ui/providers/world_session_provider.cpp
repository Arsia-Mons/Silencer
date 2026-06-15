#include "world_session_provider.h"

#include "client/ui/hooks/use_buy_tech.h"
#include "client/ui/hooks/use_hud.h"
#include "client/ui/hooks/use_ingame_chat.h"
#include "client/ui/hooks/use_match.h"
#include "client/ui/hooks/use_player_status.h"
#include "client/ui/hooks/use_teams.h"
#include "client/ui/hooks/use_tech.h"
#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext WorldSessionContext = {};

::ui::UiElement WorldSessionProvider(const WorldSessionValue &value,
                                     ::ui::UiChildren children,
                                     const char *key) {
  const WorldSessionValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store world-session context\n");
    return ::ui::empty();
  }
  return ::ui::provider("WorldSessionProvider", &WorldSessionContext,
                        const_cast<WorldSessionValue *>(stored), children, key);
}

PlayerStatus use_player_status() {
  WorldSessionValue *value =
      static_cast<WorldSessionValue *>(use_context(&WorldSessionContext));
  if (!value) {
    react_report_error("client/ui: missing WorldSessionProvider\n");
    return {};
  }
  const WorldSessionSnapshot &s = value->snapshot;
  PlayerStatus out;
  out.valid = s.player_valid;
  out.health = s.health;
  out.max_health = s.max_health;
  out.shield = s.shield;
  out.max_shield = s.max_shield;
  out.fuel = s.fuel;
  out.max_fuel = s.max_fuel;
  out.fuel_low = s.fuel_low;
  out.current_weapon = s.current_weapon;
  out.laser_ammo = s.laser_ammo;
  out.rocket_ammo = s.rocket_ammo;
  out.flamer_ammo = s.flamer_ammo;
  out.files = s.files;
  out.max_files = s.max_files;
  out.credits = s.credits;
  for (int i = 0; i < 4; ++i) {
    out.inventory_items[i] = s.inventory_items[i];
    out.inventory_counts[i] = s.inventory_counts[i];
    out.inventory_res_index[i] = s.inventory_res_index[i];
    out.inventory_letters[i] = s.inventory_letters[i];
  }
  out.current_inventory = s.current_inventory;
  out.select_inventory_slot = value->select_inventory_slot;
  return out;
}

Match use_match() {
  WorldSessionValue *value =
      static_cast<WorldSessionValue *>(use_context(&WorldSessionContext));
  if (!value) {
    react_report_error("client/ui: missing WorldSessionProvider\n");
    return {};
  }
  const WorldSessionSnapshot &s = value->snapshot;
  return {
      .valid = s.match_valid,
      .mode_id = s.mode_id,
      .mode_name = s.mode_name,
      .phase = s.match_phase,
      .time_secs = s.match_time_secs,
      .winning_team_id = s.winning_team_id,
      .scores = s.scores,
      .message = s.message,
  };
}

namespace {
// The new overlay hooks share one read of the WorldSessionContext; nullptr means
// no provider is mounted (off the in-match tree) and the hook returns a default.
WorldSessionValue *world_session_value() {
  WorldSessionValue *value =
      static_cast<WorldSessionValue *>(use_context(&WorldSessionContext));
  if (!value)
    react_report_error("client/ui: missing WorldSessionProvider\n");
  return value;
}
} // namespace

Tech use_tech() {
  WorldSessionValue *value = world_session_value();
  if (!value)
    return {};
  const WorldSessionSnapshot &s = value->snapshot;
  Tech out;
  out.buy_active = s.buy_active;
  out.tech_active = s.tech_active;
  out.items = s.buytech_items;
  out.footer = s.buytech_footer;
  out.purchase = value->buytech_purchase;
  out.close = value->buytech_close;
  return out;
}

BuyTech use_buy_tech() {
  WorldSessionValue *value = world_session_value();
  if (!value)
    return {};
  BuyTech out;
  out.selected_index = value->snapshot.buytech_selected;
  out.select = value->buytech_select;
  return out;
}

IngameChat use_ingame_chat() {
  WorldSessionValue *value = world_session_value();
  if (!value)
    return {};
  const WorldSessionSnapshot &s = value->snapshot;
  IngameChat out;
  out.active = s.chat_active;
  out.with_team = s.chat_with_team;
  out.caret_on = s.chat_caret_on;
  out.show_ticks = s.chat_show_ticks;
  out.text = s.chat_text;
  out.log = s.chat_log;
  out.set_text = value->chat_set_text;
  out.send = value->chat_send;
  out.cancel = value->chat_cancel;
  out.toggle_channel = value->chat_toggle_channel;
  return out;
}

Hud use_hud() {
  WorldSessionValue *value = world_session_value();
  if (!value)
    return {};
  const WorldSessionSnapshot &s = value->snapshot;
  Hud out;
  out.valid = s.player_valid;
  out.phase = s.hud_phase;
  out.tick = s.hud_tick;
  out.poisoned = s.poisoned;
  out.mission_over = s.mission_over;
  out.message = s.message;
  out.message_i = s.message_i;
  out.message_type = s.message_type;
  out.message_time = s.message_time;
  out.trace_time = s.trace_time;
  out.system_camera[0] = s.system_camera[0];
  out.system_camera[1] = s.system_camera[1];
  out.top_message = s.top_message;
  out.top_message_i = s.top_message_i;
  out.status_lines.reserve(s.status_lines.size());
  for (const WorldSessionSnapshot::HudStatus &line : s.status_lines)
    out.status_lines.push_back({line.text, line.time, line.color});
  out.secret.visible = s.secret.visible;
  out.secret.beaming = s.secret.beaming;
  out.secret.progress = s.secret.progress;
  out.secret.yoffset = s.secret.yoffset;
  out.secret.hacking_tick = s.secret.hacking_tick;
  out.secret.highlight_secrets = s.secret.highlight_secrets;
  out.secret.highlight_minimap = s.secret.highlight_minimap;
  out.teams.reserve(s.hud_teams.size());
  for (const WorldSessionSnapshot::HudTeamStrip &t : s.hud_teams) {
    HudTeamRow row;
    row.secrets = t.secrets;
    row.beaming = t.beaming;
    row.num_peers = t.num_peers;
    for (int i = 0; i < 4; ++i) {
      row.peers[i].present = t.peers[i].present;
      row.peers[i].dead = t.peers[i].dead;
      row.peers[i].has_secret = t.peers[i].has_secret;
      row.peers[i].sprite = t.peers[i].sprite;
      row.peers[i].name = t.peers[i].name;
      row.peers[i].level = t.peers[i].level;
      row.peers[i].endurance = t.peers[i].endurance;
      row.peers[i].shield = t.peers[i].shield;
      row.peers[i].jetpack = t.peers[i].jetpack;
      row.peers[i].hacking = t.peers[i].hacking;
      row.peers[i].contacts = t.peers[i].contacts;
    }
    row.emblem = t.emblem;
    row.emblem_w = t.emblem_w;
    row.emblem_h = t.emblem_h;
    out.teams.push_back(row);
  }
  return out;
}

Teams use_teams() {
  WorldSessionValue *value = world_session_value();
  if (!value)
    return {};
  const WorldSessionSnapshot &s = value->snapshot;
  Teams out;
  out.showing = s.show_player_list;
  out.teams.reserve(s.scores.size());
  for (size_t i = 0; i < s.scores.size(); ++i)
    out.teams.push_back({(uint8_t)i, s.scores[i]});
  out.players = s.players;
  out.set_show_player_list = value->set_show_player_list;
  out.request_peer_list = value->request_peer_list;
  return out;
}

} // namespace client::ui
