#pragma once

#include "client/ui/hooks/use_match.h"
#include "client/ui/hooks/use_player_status.h"
#include "ui/runtime/element.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// A POD copy of the in-match read-state the HUD needs, captured once per tick by
// the composition root *before* the build phase (doc §5) — the provider never
// reaches into World/Player at build time, and no raw handle crosses into the
// app shell. Populated only while in a match; otherwise default (invalid).
struct WorldSessionSnapshot {
  // --- viewed agent (use_player_status) ---
  bool player_valid = false;
  uint16_t health = 0;
  uint16_t max_health = 0;
  uint16_t shield = 0;
  uint16_t max_shield = 0;
  uint8_t fuel = 0;
  uint8_t max_fuel = 0;
  bool fuel_low = false;
  uint8_t current_weapon = 0;
  uint8_t laser_ammo = 0;
  uint8_t rocket_ammo = 0;
  uint8_t flamer_ammo = 0;
  uint16_t files = 0;
  uint16_t max_files = 0;
  uint16_t credits = 0;
  uint8_t inventory_items[4] = {0, 0, 0, 0};
  uint8_t inventory_counts[4] = {0, 0, 0, 0};
  uint8_t current_inventory = 0;

  // --- match (use_match), read from the replicated GameStateObject ---
  bool match_valid = false;
  uint8_t mode_id = 0;
  std::string mode_name = {};
  uint8_t match_phase = 0;
  uint16_t match_time_secs = 0;
  uint16_t winning_team_id = 0;
  std::vector<uint16_t> scores = {};
  std::string message = {};
};

// The in-match frame value (doc §5): the per-tick snapshot + the queued intent
// closures the composition root installs over the public Game/World/Player seam.
// use_player_status / use_match read this; screens never see it directly.
struct WorldSessionValue {
  WorldSessionSnapshot snapshot = {};
  std::function<void(int)> select_inventory_slot = {};
  std::function<void()> confirm_quit = {};
};

// Publishes the in-match model to the component tree. Mounted by the composition
// root in the global FrameProvider chain (re-mounted at each in-match overlay
// root once overlays land). Holds no game handle — only the resolved value.
::ui::UiElement WorldSessionProvider(const WorldSessionValue &value,
                                     ::ui::UiChildren children,
                                     const char *key = nullptr);

} // namespace client::ui
