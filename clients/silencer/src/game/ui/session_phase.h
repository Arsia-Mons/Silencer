#pragma once

#include "client/ui/hooks/use_session.h"
#include "game/state/game_state.h"

#include <SDL3/SDL_stdinc.h>

namespace silencer::game_ui {

// Projects the game state machine onto the retained UI's session phase. During a
// FADEOUT the phase tracks the SOURCE (`fadefromstate`), not the destination, so
// the outgoing screen stays mounted and dims to black; only once the fade reaches
// black does `state` flip to the target.
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
