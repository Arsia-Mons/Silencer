#pragma once

#include <cstdint>
#include <functional>

namespace client::ui {

// The viewed agent's vitals/armament/inventory (doc §6). A read projection of
// the spectated Player, captured per-tick by the composition root; `valid` is
// false when no player is being viewed (loading, spectating an empty slot, or
// dead). `select_inventory_slot` is the only mutating intent (queued).
struct PlayerStatus {
  bool valid = false;

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

  // Four inventory slots: item id + count, plus the selected slot.
  uint8_t inventory_items[4] = {0, 0, 0, 0};
  uint8_t inventory_counts[4] = {0, 0, 0, 0};
  uint8_t inventory_res_index[4] = {0xFF, 0xFF, 0xFF, 0xFF}; // bank-97 sprite
  char inventory_letters[4] = {0, 0, 0, 0};                  // slot letter
  uint8_t current_inventory = 0;

  std::function<void(int)> select_inventory_slot = {};
};

PlayerStatus use_player_status();

} // namespace client::ui
