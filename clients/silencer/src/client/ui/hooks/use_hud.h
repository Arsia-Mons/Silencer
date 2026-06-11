#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace client::ui {

// In-game HUD read projection (origin ui/hud parity): the pulse clock, team
// strip rows (with frame-resolved sprite variants), poison/quit flags, and the
// center-message reveal state. Captured per tick by the composition root.
struct HudTeamRow {
  uint8_t secrets = 0;
  bool beaming = false;
  int num_peers = 0;
  struct Peer {
    bool present = false;
    bool dead = false;
    bool has_secret = false;
    uint32_t sprite = 0; // pulse-resolved texture for this frame
    std::string name = {};
    int level = 0, endurance = 0, shield = 0, jetpack = 0, hacking = 0,
        contacts = 0;
  } peers[4];
  uint32_t emblem = 0; // team-colorized + outlined, 2x
  uint16_t emblem_w = 0, emblem_h = 0;
};

struct Hud {
  bool valid = false;
  uint8_t phase = 0;  // origin renderer.GetHudAnimationPhase()
  uint32_t tick = 0;  // world tick (secret flicker)
  bool poisoned = false;
  uint8_t quit_state = 0;
  std::string message = {};
  uint8_t message_i = 0, message_type = 0, message_time = 0;
  std::vector<HudTeamRow> teams = {};
};

Hud use_hud();

} // namespace client::ui
