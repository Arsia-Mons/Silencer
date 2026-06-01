#pragma once

#include "ui/runtime/element.h"

namespace client::ui {

// LobbyConnect phase view, wrapped as a keyed component. AppRoot's
// make_phase_element returns this for SessionPhase::Connecting. Pure view over
// use_lobby_session(): renders the connection log + username/password fields and
// exposes Login (submit credentials) / Cancel. The connect → version → auth →
// route orchestration runs on the game tick (LobbyConnectFlow); this screen
// neither drives it nor touches the lobby directly.
::ui::UiElement LobbyConnect(const char *key);

} // namespace client::ui
