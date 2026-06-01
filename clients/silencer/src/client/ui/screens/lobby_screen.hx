#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// Lobby phase view, wrapped as a keyed component. AppRoot's make_phase_element
// returns this for SessionPhase::Lobby. Agent + Chat panels (always on) plus a
// screen-local right-column swap (SIL-21 (3/n)): GameSelect (browse +
// join/spectate/new) -> GameCreate (name + bundled-map cycle) -> Staging (roster
// + ready/team/leave), auto-showing Staging once connected (use_staging.active).
::ui::UiElement LobbyScreen(const char *key);

} // namespace client::ui
