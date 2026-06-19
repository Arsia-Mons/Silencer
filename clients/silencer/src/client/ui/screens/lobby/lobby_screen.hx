#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// Lobby phase view. AppRoot's make_phase_element returns this for
// SessionPhase::Lobby.
::ui::UiElement LobbyScreen(const char *key);

} // namespace client::ui
