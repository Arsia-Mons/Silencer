#pragma once

#include "client/ui/hooks/use_session.h"
#include "game/state/game_state.h"

#include <SDL3/SDL_stdinc.h>

namespace silencer::game_ui {

// Projects the live game state machine (game.cpp `state`) onto the retained
// UI's session phase (doc §6). `state` is a GameState::* value; `nextstate` is
// the pending target the FADEOUT transition is heading toward, so while a fade
// masks the switch the phase already tracks the destination (the retained UI
// has no separate fade state to dwell in).
//
// Notes on the non-1:1 cases:
//   * OPTIONS* are Tier-1 overlays over the menu in the retained model
//     (SIL-19); until those land, the phase *under* them is MainMenu.
//   * HOST/JOIN are the connect-and-load path -> Loading; TEST/REPLAY are
//     in-match variants -> InMatch.
//   * NONE (boot) falls back to MainMenu.
inline client::ui::SessionPhase project_session_phase(Uint8 state,
                                                      Uint8 nextstate) {
  using namespace GameState;
  const Uint8 effective = (state == FADEOUT) ? nextstate : state;
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
  case OPTIONS:
  case OPTIONSCONTROLS:
  case OPTIONSDISPLAY:
  case OPTIONSAUDIO:
  case NONE:
  default:
    return client::ui::SessionPhase::MainMenu;
  }
}

} // namespace silencer::game_ui
