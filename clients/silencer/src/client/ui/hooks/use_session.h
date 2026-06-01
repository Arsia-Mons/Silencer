#pragma once

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

// SIL-14 carries only `phase` — the slice the AppRoot reconciler needs. SIL-15
// grows this with `authenticated`/`paused`/`is_live_multiplayer`/
// `current_game_id` + the named session intents (doc §6).
struct Session {
  SessionPhase phase = SessionPhase::MainMenu;
};

Session use_session();

} // namespace client::ui
