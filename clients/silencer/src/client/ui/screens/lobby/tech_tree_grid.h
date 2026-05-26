#ifndef SILENCER_CLIENT_UI_LOBBY_TECH_TREE_GRID_H
#define SILENCER_CLIENT_UI_LOBBY_TECH_TREE_GRID_H

// 4-column tech-choice grid for the lobby GameTechPanel. Three remote-peer
// columns + one local column with tech-name labels + per-row description
// hit-target widget registration. Domain state is supplied by GameTechPanelState.

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::lobby {

struct GameTechPanelState;

void BuildTechTreeGrid(const GameTechPanelState & state,
                       silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
