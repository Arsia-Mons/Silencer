#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// One peer in the pre-match staging room (doc §6). Display fields are
// pre-formatted by the composition root; `is_local` marks the viewing player's
// row, `ready` reflects the peer's ready flag, `team_number` groups rows.
struct StagingRosterRow {
  std::string name = {};  // display name (+ " [BOT]" for bots, like origin)
  std::string level = {}; // "L:n" (origin Tiny palette-170 badge)
  uint8_t team_number = 0;
  uint8_t peer_slot = 0;  // slot within the team (origin kRosterPeerStepY)
  uint8_t agency = 0;     // team agency (emblem index)
  bool draw_emblem = false; // first row of each team carries the emblem
  bool ready = false;
  bool is_local = false;
};

// The pre-match room model (doc §6): the roster + ready/team/leave intents over
// the §7a public World seam (SendReady / ChangeTeam / IsInLobby /
// AllPeersDownloadedMap). `ready_blocked` mirrors the host-side guard
// (is_host && !AllPeersDownloadedMap) so the screen can label/disable Ready;
// `ready_label` is the pre-resolved button text. `active` is true while
// connected to a game (staging or playing); `in_game_lobby` while still
// pre-match. Tech loadout (slots/buyable/wanted + set/toggle) joins in
// SIL-21 (4/n).
// One selectable pre-match tech (origin tech_tree_grid row): the GAS buyable's
// name + slot cost, whether the local peer has it chosen, and whether it can be
// toggled (enough slots left, or already chosen so it can be un-chosen).
struct StagingTechRow {
  std::string name = {};
  std::string description_title = {};
  std::string description = {};
  uint8_t slots = 0;
  uint32_t choice_mask = 0;
  bool selected = false;
  bool interactable = false;
};

struct Staging {
  bool active = false;
  bool in_game_lobby = false;
  bool is_host = false;
  bool ready_blocked = false;
  std::string ready_label = "Ready";
  std::string map_name = {}; // origin shows it in the lobby title bar
  std::vector<StagingRosterRow> roster = {};
  // Pre-match tech loadout (origin GameTechPanel): the selectable techs, the
  // local choice bitmask, and the slots-left readout.
  std::vector<StagingTechRow> tech = {};
  uint32_t tech_choices = 0;
  std::string tech_slots_label = {};

  std::function<void()> send_ready = {};
  std::function<void(uint32_t)> set_tech = {};
  std::function<void()> change_team = {};
  std::function<void()> leave = {};
};

Staging use_staging();

} // namespace client::ui
