#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// One team's running score (doc §6). `number` is the 0-based team index; `score`
// the replicated per-team objective score from the GameStateObject.
struct TeamScore {
  uint8_t number = 0;
  uint16_t score = 0;
};

// One scoreboard occupant (doc §6): the display name of a connected user, the
// team they belong to, and whether it is the local player.
struct ScoreboardPlayer {
  std::string name = {};
  uint8_t team_number = 0;
  bool local = false;
};

// The teams / scoreboard model (doc §6): per-team scores + the player roster
// (read) plus the player-list toggle and peer-list request over the public World
// seam (`SetShowingPlayerList`, `RequestPeerList`). `showing` reflects whether
// the scoreboard overlay is up.
struct Teams {
  bool showing = false;
  std::vector<TeamScore> teams = {};
  std::vector<ScoreboardPlayer> players = {};

  std::function<void(bool)> set_show_player_list = {};
  std::function<void()> request_peer_list = {};
};

Teams use_teams();

} // namespace client::ui
