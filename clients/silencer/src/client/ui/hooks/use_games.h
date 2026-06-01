#pragma once

#include <string>

namespace client::ui {

// The games-browser model (doc §6): the open games the LobbyScreen's
// GameSelectPanel lists. SIL-21 (2/n) is the read view (a newline-joined list);
// the id-based join/spectate/create intents + the game-join pump land in (3/n).
struct Games {
  std::string list = {};
};

Games use_games();

} // namespace client::ui
