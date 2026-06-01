#pragma once

#include <functional>
#include <string>

namespace client::ui {

// The lobby chat model (doc §6): the chat scrollback + online presence the
// LobbyScreen's ChatPanel shows, plus the send intent. The scrollback is
// drained from the lobby's message queue on the game tick (a sibling of
// DoNetwork) into a buffer the snapshot exposes here as recent, newline-joined
// tails. `send` posts to the current channel over the public lobby seam,
// queued + drained after render. (Channel switching joins in a later sub-slice.)
struct LobbyChat {
  std::string scrollback = {};
  std::string presence = {};

  std::function<void(const std::string &)> send = {};
};

LobbyChat use_lobby_chat();

} // namespace client::ui
