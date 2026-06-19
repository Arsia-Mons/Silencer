#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// The character roster model: the agents the account owns + the create/select
// intents the CharacterCreate wizard drives. `received` is false while the
// roster is still in flight from the lobby.
struct CharacterProfile {
  std::string name = {};
  int agency_index = 0;
  bool rename_available = false;
  uint16_t wins = 0;
  uint16_t losses = 0;
  uint16_t xp = 0;
  uint8_t level = 0;
  uint8_t endurance = 0;
  uint8_t shield = 0;
  uint8_t jetpack = 0;
  uint8_t techslots = 0;
  uint8_t hacking = 0;
  uint8_t contacts = 0;
};

struct Characters {
  std::vector<std::string> roster = {};
  std::vector<CharacterProfile> profiles = {};
  bool received = false;
  // The selected agent's name + compact stat summary (empty until loaded).
  std::string selected_summary = {};

  std::function<void(const std::string &, int)> create = {}; // (alias, agency 0..4)
  std::function<void(int)> select = {};                       // existing, by roster index
};

Characters use_characters();

} // namespace client::ui
