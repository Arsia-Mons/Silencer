#ifndef SILENCER_CLIENT_UI_LOBBY_TECH_SELECTED_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_TECH_SELECTED_PANEL_H

// Lobby GameTechPanel "selected tech" detail block: centered tech-name
// heading + 8 description lines, fed by GameTechPanelState.

namespace silencer::client_ui::lobby {

struct GameTechPanelState;

void BuildTechSelectedPanel(const GameTechPanelState & state);

}  // namespace silencer::client_ui::lobby

#endif
