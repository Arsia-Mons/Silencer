#pragma once

#include <functional>
#include <string>

namespace client::ui {

// The in-match chat model (doc §6): compose state for the viewed agent plus the
// send/cancel/toggle intents over the public `World::SendChat` seam. `active` is
// the compose-open flag; `with_team` the channel (team vs all); `show_ticks` the
// HUD scrollback countdown (>0 means the last message is still visible); `text`
// the message currently being composed. `send(text)` transmits + closes compose;
// `cancel()` closes without sending; `toggle_channel()` flips team/all.
struct IngameChat {
  bool active = false;
  bool with_team = false;
  int show_ticks = 0;
  std::string text = {};

  std::function<void(const std::string &)> send = {};
  std::function<void()> cancel = {};
  std::function<void()> toggle_channel = {};
};

IngameChat use_ingame_chat();

} // namespace client::ui
