#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace client::ui {

// In-game HUD read projection (origin ui/hud parity): the pulse clock, team
// strip rows (with frame-resolved sprite variants), poison + mission-over
// flags, and the center-message reveal state. Captured per tick by the
// composition root.
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

// Bottom-center fading status stack (origin WorldMessaging statusmessages):
// newest first; `time` is the remaining ticks (fade-out under 16).
struct HudStatusLine {
  std::string text;
  uint8_t time = 0;
  uint8_t color = 0;
};

// Team-base hack panel (origin hud_secret_overlays.cpp). Shown whenever the
// viewed player's team has a base door; progress lines hidden while beaming.
struct HudSecretOverlay {
  bool visible = false;
  bool beaming = false;
  int progress = 0;     // team secretprogress (20 per line)
  int yoffset = 60;     // 60, + teams*20-65 when >=3 teams
  bool hacking_tick = false; // player HACKING at the threshold-shift tick
  // Pulse-resolved highlight flash textures (0 = flag off this frame).
  uint32_t highlight_secrets = 0, highlight_minimap = 0;
};

struct Hud {
  bool valid = false;
  uint8_t phase = 0;  // origin renderer.GetHudAnimationPhase()
  uint32_t tick = 0;  // world tick (secret flicker)
  bool poisoned = false;
  bool mission_over = false; // END_MISSION reached => InGameScreen pushes PauseScreen
  std::string message = {};
  uint8_t message_i = 0, message_type = 0, message_time = 0;
  uint8_t trace_time = 0; // "Government Trace Time: N" when > 0
  bool system_camera[2] = {false, false};
  std::string top_message = {};
  uint8_t top_message_i = 0;
  std::vector<HudStatusLine> status_lines = {};
  HudSecretOverlay secret = {};
  std::vector<HudTeamRow> teams = {};
};

Hud use_hud();

} // namespace client::ui
