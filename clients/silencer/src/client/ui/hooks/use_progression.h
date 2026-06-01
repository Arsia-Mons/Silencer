#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace client::ui {

// Post-match progression model (doc §6): the mission stats the summary screen
// shows + per-stat upgrade and the exit intent. The composition root computes
// the snapshot once per tick from the post-match `User` stats; the intents route
// to the public lobby seam (UpgradeStat / GoToState), queued + drained after
// render. The six stats are, in order:
//   0 Endurance · 1 Shield · 2 Jetpack · 3 Tech Slots · 4 Hacking · 5 Contacts
struct Progression {
  // False until the post-match stats have arrived from the lobby (the user
  // record is still `retrieving`). The screen shows a loading line until then.
  bool loaded = false;
  uint32_t experience = 0;
  // The full per-mission stat breakdown (kills/deaths/secrets/…), newline-joined
  // for display.
  std::string stats_text = {};
  // True when the player has upgrade points to spend this session.
  bool upgrade_banner = false;
  // Current upgrade level of each of the six stats.
  uint8_t levels[6] = {0, 0, 0, 0, 0, 0};
  // Whether each stat can still be upgraded (below its agency max) — only
  // meaningful when `upgrade_banner` is set.
  bool upgrades_available[6] = {false, false, false, false, false, false};

  // Spend an upgrade point on stat `index` (0..5). Queued, drained after render.
  std::function<void(int)> upgrade = {};
  // Leave the summary: rejoin the lobby if still authenticated, else the menu.
  std::function<void()> finish = {};
};

Progression use_progression();

} // namespace client::ui
