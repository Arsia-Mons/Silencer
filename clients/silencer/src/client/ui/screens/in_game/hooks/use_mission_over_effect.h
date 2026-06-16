#pragma once

#include "client/ui/hooks/use_hud.h"        // Hud
#include "client/ui/hooks/use_navigation.h" // Navigation
#include "client/ui/screens/pause_screen.h" // PauseScreen
#include "ui/runtime/react.h"               // use_state

#include <memory>

namespace client::ui {

// Scripted mission-end (END_MISSION) pushes the PauseScreen once on the rising
// edge of the projected mission-over flag — not a per-frame imperative check.
// nav.push queues a deferred mutation, so this is build-safe. Owns its edge cell.
inline void use_mission_over_effect(const Hud &hud, const Navigation &nav) {
  bool *prev_mission_over = use_state<bool>(false);
  if (!prev_mission_over)
    return;
  if (hud.mission_over && !*prev_mission_over && nav.push)
    nav.push(std::make_unique<PauseScreen>());
  *prev_mission_over = hud.mission_over;
}

} // namespace client::ui
