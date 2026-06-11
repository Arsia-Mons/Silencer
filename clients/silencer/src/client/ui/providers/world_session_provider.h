#pragma once

#include "client/ui/hooks/use_buy_tech.h"
#include "client/ui/hooks/use_ingame_chat.h"
#include "client/ui/hooks/use_match.h"
#include "client/ui/hooks/use_player_status.h"
#include "client/ui/hooks/use_teams.h"
#include "client/ui/hooks/use_tech.h"
#include "ui/runtime/element.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// A POD copy of the in-match read-state the HUD needs, captured once per tick by
// the composition root *before* the build phase (doc §5) — the provider never
// reaches into World/Player at build time, and no raw handle crosses into the
// app shell. Populated only while in a match; otherwise default (invalid).
struct WorldSessionSnapshot {
  // --- viewed agent (use_player_status) ---
  bool player_valid = false;
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
  uint8_t inventory_items[4] = {0, 0, 0, 0};
  uint8_t inventory_counts[4] = {0, 0, 0, 0};
  // Render projections of the slot item ids (origin Renderer::InvIdToResIndex /
  // InvIdToLetter): bank-97 sprite index (0xFF = none) + slot letter (0 = none).
  uint8_t inventory_res_index[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  char inventory_letters[4] = {0, 0, 0, 0};
  uint8_t current_inventory = 0;

  // --- match (use_match), read from the replicated GameStateObject ---
  bool match_valid = false;
  uint8_t mode_id = 0;
  std::string mode_name = {};
  uint8_t match_phase = 0;
  uint16_t match_time_secs = 0;
  uint16_t winning_team_id = 0;
  std::vector<uint16_t> scores = {};
  std::string message = {};

  // --- buy/tech station overlay (use_tech / use_buy_tech) ---
  // The viewed agent has at most one station open (buy XOR tech). `buytech_items`
  // is the open station's rows; `buytech_selected` the replicated cursor
  // (Player::buyifacelastitem / techifacelastitem).
  bool buy_active = false;
  bool tech_active = false;
  int buytech_selected = 0;
  std::vector<TechItem> buytech_items = {};
  std::string buytech_footer = {};

  // --- in-game chat overlay (use_ingame_chat) ---
  bool chat_active = false;
  bool chat_with_team = false;
  bool chat_caret_on = false; // origin blink: (SDL_GetTicks()/50)%32 < 16
  int chat_show_ticks = 0;
  std::string chat_text = {};
  std::vector<std::string> chat_log = {}; // recent scrollback, oldest -> newest

  // --- player list / scoreboard (use_teams) ---
  // Per-team scores reuse `scores` above; `players` is the connected roster.
  bool show_player_list = false;
  std::vector<ScoreboardPlayer> players = {};

  // --- in-game HUD (use_hud, origin ui/hud parity) ---
  // Per-team strip rows. `peers[].sprite` is the pulse-resolved texture for
  // this frame (in-base / has-secret ramps applied game-side); `emblem` is the
  // team-colorized, outlined, 2x emblem.
  struct HudTeamStrip {
    uint8_t secrets = 0;
    bool beaming = false;
    int num_peers = 0;
    struct Peer {
      bool present = false;
      bool dead = false;
      bool has_secret = false;
      uint32_t sprite = 0;
      // Player-list (F1) row data: display name + the user's per-agency
      // profile stats (origin HudView TeamPeerView).
      std::string name = {};
      int level = 0, endurance = 0, shield = 0, jetpack = 0, hacking = 0,
          contacts = 0;
    } peers[4];
    uint32_t emblem = 0;
    uint16_t emblem_w = 0, emblem_h = 0;
  };
  uint8_t hud_phase = 0;  // renderer pulse clock (+1/sim frame)
  uint32_t hud_tick = 0;  // world tick (secret-slot flicker)
  bool poisoned = false;
  uint8_t quit_state = 0; // 1/2 => "Hit Enter To Quit"
  uint8_t message_i = 0, message_type = 0, message_time = 0;
  std::vector<HudTeamStrip> hud_teams = {};
};

// The in-match frame value (doc §5): the per-tick snapshot + the queued intent
// closures the composition root installs over the public Game/World/Player seam.
// use_player_status / use_match read this; screens never see it directly.
struct WorldSessionValue {
  WorldSessionSnapshot snapshot = {};
  std::function<void(int)> select_inventory_slot = {};
  std::function<void()> confirm_quit = {};

  // Buy/tech station intents (use_tech / use_buy_tech).
  std::function<void(int)> buytech_select = {};
  std::function<void(int)> buytech_purchase = {};
  std::function<void()> buytech_close = {};

  // In-game chat intents (use_ingame_chat).
  std::function<void(const std::string &)> chat_set_text = {};
  std::function<void(const std::string &)> chat_send = {};
  std::function<void()> chat_cancel = {};
  std::function<void()> chat_toggle_channel = {};

  // Teams / scoreboard intents (use_teams).
  std::function<void(bool)> set_show_player_list = {};
  std::function<void()> request_peer_list = {};
};

// Publishes the in-match model to the component tree. Mounted by the composition
// root in the global FrameProvider chain (re-mounted at each in-match overlay
// root once overlays land). Holds no game handle — only the resolved value.
::ui::UiElement WorldSessionProvider(const WorldSessionValue &value,
                                     ::ui::UiChildren children,
                                     const char *key = nullptr);

} // namespace client::ui
