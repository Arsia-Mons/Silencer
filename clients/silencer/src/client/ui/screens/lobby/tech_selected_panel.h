#ifndef SILENCER_CLIENT_UI_LOBBY_TECH_SELECTED_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_TECH_SELECTED_PANEL_H

// Lobby GameTechPanel "selected tech" detail block: centered tech-name
// heading + 8 description lines, fed by lobby hooks.

#include "hooks/use_lobby.h"

namespace silencer::client_ui::lobby {

void BuildTechSelectedPanel(const hooks::LobbyTechItemDetails & details);

}  // namespace silencer::client_ui::lobby

#endif
