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
  std::vector<CharacterProfile> character_profiles = {};

  // --- lobby (LobbyScreen, read panels) — recent tails, newline-joined ---
  std::string lobby_agent = {};    // selected agent name + compact summary
  std::string lobby_chat = {};     // chat scrollback (drained on the tick)
  std::string lobby_presence = {}; // who's online
  std::string chat_channel = {};   // lobby.channel — the game channel in staging

  // --- games browser (LobbyScreen GameSelectPanel) ---
  std::vector<GameBrowserEntry> games = {}; // open games (structured)
  std::vector<std::string> bundled_maps = {}; // create-form map choices

  // --- create/join in flight (SIL-234) — masks the browser through the async
  // connect so the right column doesn't flash the games browser between the
  // create form and staging. True from the Create/Join click until the connect
  // settles (staging takes over on connect; cleared on idle/failure). ---
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
  // The per-mission stat breakdown, one display line per entry — origin's
  // summaryLines verbatim (value right-padded into a 30-char Body column).
  std::vector<std::string> summary_lines = {};
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
  // Games browser: id-based join / spectate / create over the public seam
  // (queued; the LOBBY-tick pump drives the connect → staging transition).
  std::function<void(uint32_t, const std::string &)> join_game = {};
  std::function<void(uint32_t)> spectate_game = {};
  std::function<void(const CreateGameRequest &)> create_game = {};
  // Staging room: ready / change-team / leave / tech loadout over the §7a
  // public World seam.
  std::function<void()> send_ready = {};
  std::function<void()> change_team = {};
  std::function<void()> leave_game = {};
  std::function<void(uint32_t)> set_tech = {};
};

// Publishes the lobby model to the component tree. Mounted in the global
// FrameProvider chain by the composition root, which assembles the value once
// per tick (snapshot + intents) and hands it in. The provider holds no game
// handle — only the resolved value.
::ui::UiElement LobbyProvider(const LobbyProviderValue &value,
                              ::ui::UiChildren children, const char *key = nullptr);

} // namespace client::ui
