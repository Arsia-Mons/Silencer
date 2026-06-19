#pragma once

#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// The lobby chat model (doc §6): the chat scrollback + online presence the
// LobbyScreen's ChatPanel shows, plus the send intent. The scrollback is
// drained from the lobby's message queue on the game tick (a sibling of
// DoNetwork) into a buffer the snapshot exposes here as a bounded message ring
// (oldest -> newest); the ChatLog virtualizes it per message. `send` posts to
// the current channel over the public lobby seam, queued + drained after render.
struct LobbyChat {
  std::vector<std::string> messages = {}; // oldest -> newest, virtualized per row
  std::string presence = {};
  std::string channel = {}; // panel header: "Lobby", or the game channel in staging

  std::function<void(const std::string &)> send = {};
};

LobbyChat use_lobby_chat();

} // namespace client::ui
