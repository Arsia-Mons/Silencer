#pragma once

#include "client/ui/hooks/use_session.h"
#include "game/state/game_state.h"

#include <SDL3/SDL_stdinc.h>

namespace silencer::game_ui {

// Projects the live game state machine (game.cpp `state`) onto the retained
// UI's session phase (doc §6). `state` is a GameState::* value; `fadefromstate`
// is the state the in-flight FADEOUT transition is leaving. While the fade
// masks the switch the outgoing screen must stay mounted and dim to black
// (legacy retained its world UI objects across FADEOUT) — so during FADEOUT the
// phase tracks the SOURCE, not the destination. Only once TickFadeOut reaches
// black does `state` flip to the target and the new screen fade in.
//
// Notes on the non-1:1 cases:
//   * Options is a Tier-1 OverlayScreen pushed above MainMenu, not a game
//     state — it never changes `state`, so the phase under it stays MainMenu.
//   * HOST/JOIN are the connect-and-load path -> Loading; TEST/REPLAY are
//     in-match variants -> InMatch.
//   * NONE (boot) falls back to MainMenu.
inline client::ui::SessionPhase project_session_phase(Uint8 state,
                                                      Uint8 fadefromstate) {
  using namespace GameState;
  const Uint8 effective = (state == FADEOUT) ? fadefromstate : state;
  switch (effective) {
  case MAINMENU:
    return client::ui::SessionPhase::MainMenu;
  case LOBBYCONNECT:
    return client::ui::SessionPhase::Connecting;
  case LOBBY:
    return client::ui::SessionPhase::Lobby;
  case UPDATING:
    return client::ui::SessionPhase::Updating;
  case INGAME:
  case TESTGAME:
  case REPLAYGAME:
    return client::ui::SessionPhase::InMatch;
  case MISSIONSUMMARY:
    return client::ui::SessionPhase::PostMatch;
  case SINGLEPLAYERGAME:
    return client::ui::SessionPhase::SinglePlayer;
  case CREATECHARACTER:
    return client::ui::SessionPhase::CharacterCreate;
  case HOSTGAME:
  case JOINGAME:
    return client::ui::SessionPhase::Loading;
  case NONE:
  default:
    return client::ui::SessionPhase::MainMenu;
  }
}

} // namespace silencer::game_ui
