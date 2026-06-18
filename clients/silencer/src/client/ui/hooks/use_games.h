#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// One open game in the lobby browser (doc §6). The display fields are
// pre-formatted by the composition root (the screen never re-derives them);
// `joinable` / `spectatable` gate the row's actions, `password_protected` flags
// a locked game. `id` is the lobby game id the join/spectate intents take.
//
// The selection info block mirrors origin's game_select_panel RecomputeInfoBlock:
// five Body lines (game name, "Map: <name>", "<Sec> Security", "Creator: <name>",
// and "MinLv:.. MaxLv:.. MaxPl:.. MaxTm:.." while pre-game). Pre-formatted here so
// the screen renders the selected entry's lines verbatim.
struct GameBrowserEntry {
  uint32_t id = 0;
  std::string name = {};
  std::string info_map = {};      // "Map: <mapname>"
  std::string info_security = {}; // "<No/Low/Medium/High> Security[  *PASSWORD LOCK*]"
  std::string info_creator = {};  // "Creator: <name>"
  std::string info_limits = {};   // "MinLv:N MaxLv:N MaxPl:N MaxTm:N" (empty in-game)
  bool joinable = false;
  bool spectatable = false;
  bool password_protected = false;
};

// A create-game request the GameCreatePanel assembles (doc §6). `map` is a
// bundled map filename (from Games::bundled_maps); the composition root hashes
// it and sends MSG_NEWGAME. The security/level/player knobs default to a sane
// quick-create; the full options form lands later.
struct CreateGameRequest {
  std::string name = {};
  std::string map = {};
  std::string password = {};
  uint8_t security = 2; // LobbyGame::SECMEDIUM
  uint8_t min_level = 0;
  uint8_t max_level = 99;
  uint8_t max_players = 24;
  uint8_t max_teams = 6;
  bool spectatable = true;
};

// The games-browser model (doc §6): the open games + the single id-based
// join/spectate/create intents (no LobbyGame leak). Intents queue deferred
// mutations over the public lobby/Game seam; the game-join pump (LOBBY tick)
// drives the connect → staging transition. `bundled_maps` feeds the
// create-form map picker. (Server-map discovery / upload — use_map_downloader —
// joins later.)
struct Games {
  std::vector<GameBrowserEntry> entries = {};
  std::vector<std::string> bundled_maps = {};

  std::function<void(uint32_t, const std::string &)> join_game = {}; // (id, password)
  std::function<void(uint32_t)> spectate_game = {};
  std::function<void(const CreateGameRequest &)> create_game = {};
  std::function<bool(uint32_t)> can_join = {};
};

Games use_games();

} // namespace client::ui
