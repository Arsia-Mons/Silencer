#pragma once

#include "client/ui/hooks/use_characters.h"
#include "client/ui/hooks/use_games.h"
#include "client/ui/hooks/use_lobby_session.h"
#include "client/ui/hooks/use_progression.h"
#include "client/ui/hooks/use_staging.h"
#include "ui/runtime/element.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// A POD copy of the lobby read-state, captured once per tick under LockMutex
// *before* build — the provider never locks at build time and no raw
// Lobby*/mutex crosses into the app shell.
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
  std::vector<CharacterProfile> character_profiles = {};

  // --- lobby (LobbyScreen, read panels) — recent tails, newline-joined ---
  std::string lobby_agent = {};    // selected agent name + compact summary
  std::vector<std::string> lobby_chat = {}; // chat scrollback (bounded ring, oldest -> newest), virtualized per-message
  std::string lobby_presence = {}; // who's online
  std::string chat_channel = {};   // lobby.channel — the game channel in staging

  // --- games browser (LobbyScreen GameSelectPanel) ---
  std::vector<GameBrowserEntry> games = {}; // open games (structured)
  std::vector<std::string> bundled_maps = {}; // create-form map choices

  // --- create/join in flight: masks the browser through the async connect so
  // the right column doesn't flash the games browser. ---
  bool joining = false;

  // --- staging room (LobbyScreen GameJoinPanel) — built when connected ---
  bool staging_active = false;       // connected to a game (staging or playing)
  bool staging_in_lobby = false;     // World::IsInLobby() — still pre-match
  bool staging_is_host = false;
  bool staging_ready_blocked = false;
  std::string staging_ready_label = "Ready";
  std::string staging_map_name = {}; // joined game's map (origin title-bar overlay)
  std::vector<StagingRosterRow> staging_roster = {};
  std::vector<StagingTechRow> staging_tech = {};
  uint32_t staging_tech_choices = 0;
  std::string staging_tech_slots_label = {};

  // --- progression (MissionSummary) ---
  bool progression_loaded = false;
  uint32_t experience = 0;
  // Per-mission stat breakdown, one display line per entry (value right-padded
  // into a 30-char column).
  std::vector<std::string> summary_lines = {};
  bool upgrade_banner = false;
  uint8_t levels[6] = {0, 0, 0, 0, 0, 0};
  bool upgrades_available[6] = {false, false, false, false, false, false};
};

// The lobby frame value: the per-tick snapshot + the queued intent closures.
// The lobby hooks read this; screens never see it directly.
struct LobbyProviderValue {
  LobbySnapshot snapshot = {};
  std::function<void(const std::string &, const std::string &)> connect = {};
  std::function<void()> cancel = {};
  std::function<void(int)> upgrade = {};
  std::function<void()> finish = {};
  // CharacterCreate: create (alias, agency 0..4) / select by roster index.
  std::function<void(const std::string &, int)> create_character = {};
  std::function<void(int)> select_character = {};
  std::function<void(const std::string &)> send_chat = {};
  // Games browser: id-based join / spectate / create.
  std::function<void(uint32_t, const std::string &)> join_game = {};
  std::function<void(uint32_t)> spectate_game = {};
  std::function<void(const CreateGameRequest &)> create_game = {};
  // Staging room.
  std::function<void()> send_ready = {};
  std::function<void()> change_team = {};
  std::function<void()> leave_game = {};
  std::function<void(uint32_t)> set_tech = {};
};

// Publishes the lobby model to the component tree; holds no game handle.
::ui::UiElement LobbyProvider(const LobbyProviderValue &value,
                              ::ui::UiChildren children, const char *key = nullptr);

} // namespace client::ui
