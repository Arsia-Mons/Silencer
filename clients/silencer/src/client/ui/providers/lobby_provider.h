#pragma once

#include "client/ui/hooks/use_characters.h"
#include "client/ui/hooks/use_lobby_session.h"
#include "client/ui/hooks/use_progression.h"
#include "ui/runtime/element.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// A POD copy of the lobby read-state the UI needs, captured once per tick under
// LockMutex by the composition root *before* the build phase (doc §5) — the
// provider never locks at build time, and no raw Lobby*/mutex crosses into the
// app shell. Populated only during lobby-relevant phases; otherwise default.
struct LobbySnapshot {
  // --- connect (LobbyConnect) ---
  std::string status_log = {};
  bool connecting = false;
  bool awaiting_credentials = false;
  bool credentials_pending = false;
  bool authenticated = false;

  // --- character roster (CharacterCreate / lobby CharacterPanel) ---
  bool characters_received = false;
  std::vector<std::string> character_names = {};

  // --- lobby (LobbyScreen, read panels) — recent tails, newline-joined ---
  std::string lobby_agent = {};    // selected agent name + compact summary
  std::string lobby_chat = {};     // chat scrollback (drained on the tick)
  std::string lobby_presence = {}; // who's online
  std::string lobby_games = {};    // open games (browser)

  // --- progression (MissionSummary) ---
  bool progression_loaded = false;
  uint32_t experience = 0;
  std::string stats_text = {};
  bool upgrade_banner = false;
  uint8_t levels[6] = {0, 0, 0, 0, 0, 0};
  bool upgrades_available[6] = {false, false, false, false, false, false};
};

// The lobby frame value (doc §5): the per-tick snapshot + the queued intent
// closures the composition root installs over the public Game/World/Lobby seam.
// The lobby hooks (use_lobby_session / use_progression) read this; screens never
// see it directly.
struct LobbyProviderValue {
  LobbySnapshot snapshot = {};
  std::function<void(const std::string &, const std::string &)> connect = {};
  std::function<void()> cancel = {};
  std::function<void(int)> upgrade = {};
  std::function<void()> finish = {};
  // CharacterCreate: create a new agent (alias, agency index 0..4) / select an
  // existing one by roster index (routes to the lobby).
  std::function<void(const std::string &, int)> create_character = {};
  std::function<void(int)> select_character = {};
  // Lobby: post a chat message to the current channel.
  std::function<void(const std::string &)> send_chat = {};
};

// Publishes the lobby model to the component tree. Mounted in the global
// FrameProvider chain by the composition root, which assembles the value once
// per tick (snapshot + intents) and hands it in. The provider holds no game
// handle — only the resolved value.
::ui::UiElement LobbyProvider(const LobbyProviderValue &value,
                              ::ui::UiChildren children, const char *key = nullptr);

} // namespace client::ui
