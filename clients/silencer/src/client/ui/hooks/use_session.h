#pragma once

#include <cstdint>
#include <functional>

namespace client::ui {

// The app's top-level mode. A read-only projection of the game's own state
// machine (game.cpp `state`), surfaced to the UI so the always-mounted AppRoot
// can map each phase onto the screen that owns it. Phases are mutually
// exclusive — at most one phase screen is live at a time. Tier-1 navigation
// (options/pause submenus, modals) layers overlays *above* the phase screen
// and does not change `phase`.
enum class SessionPhase {
  MainMenu,
  Connecting,
  CharacterCreate,
  Lobby,
  Updating,
  Loading,
  InMatch,
  PostMatch,
  SinglePlayer,
};

// The session model (doc §6): cohesive read state + named intents. `phase` is
// the read-only projection AppRoot's reconciler consumes; the intents are the
// high-level transitions the menus drive (they route to the public Game/World
// command seam — GoToState/LeaveJoinedGame/paused — installed by the
// composition root, never a screen reaching into Game). They take effect for
// the UI on the next frame via the phase reconciler.
struct Session {
  SessionPhase phase = SessionPhase::MainMenu;
  bool authenticated = false;
  bool paused = false;
  bool is_live_multiplayer = false;
  uint32_t current_game_id = 0;

  std::function<void()> play_online = {};
  std::function<void()> start_tutorial = {};
  std::function<void()> open_character_create = {};
  std::function<void()> leave_match = {};
  std::function<void()> leave_to_menu = {};
  std::function<void()> leave_to_previous = {};
  std::function<void(bool)> set_paused = {};
};

Session use_session();

} // namespace client::ui
