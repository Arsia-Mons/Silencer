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
  std::string name = {};   // display name (+ " (you)" / " [BOT]")
  std::string detail = {}; // "Agency · L:n"
  uint8_t team_number = 0;
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
struct Staging {
  bool active = false;
  bool in_game_lobby = false;
  bool is_host = false;
  bool ready_blocked = false;
  std::string ready_label = "Ready";
  std::string map_name = {}; // origin shows it in the lobby title bar
  std::vector<StagingRosterRow> roster = {};

  std::function<void()> send_ready = {};
  std::function<void()> change_team = {};
  std::function<void()> leave = {};
};

Staging use_staging();

} // namespace client::ui
