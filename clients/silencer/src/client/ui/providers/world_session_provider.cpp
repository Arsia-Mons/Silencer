#include "world_session_provider.h"

#include "client/ui/hooks/use_match.h"
#include "client/ui/hooks/use_player_status.h"
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
      .confirm_quit = value->confirm_quit,
  };
}

} // namespace client::ui
