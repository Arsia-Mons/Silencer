#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// Lobby phase view, wrapped as a keyed component. AppRoot's make_phase_element
// returns this for SessionPhase::Lobby. SIL-21 (2/n) is the read cluster:
// CharacterPanel (selected agent) + ChatPanel (scrollback + presence + send) +
// GameSelectPanel (open games, read). The id-based join/spectate/create panels
// + the staging room + the game-join pump land in (3/n).
::ui::UiElement LobbyScreen(const char *key);

} // namespace client::ui
