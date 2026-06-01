#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// The match model (doc §6): mode / phase / score / objective message, read from
// the replicated GameStateObject (so it works for both authority and replicas).
// `valid` gates rendering while the match state object is absent (during
// loading). `confirm_quit` leaves the match (queued).
struct Match {
  bool valid = false;
  uint8_t mode_id = 0;
  std::string mode_name = {};
  uint8_t phase = 0; // 0 = warmup, 1 = active, 2 = over
  uint16_t time_secs = 0;
  uint16_t winning_team_id = 0; // 0 = none, 0xFFFF = draw
  std::vector<uint16_t> scores = {};
  std::string message = {}; // current center status message

  std::function<void()> confirm_quit = {};
};

Match use_match();

} // namespace client::ui
