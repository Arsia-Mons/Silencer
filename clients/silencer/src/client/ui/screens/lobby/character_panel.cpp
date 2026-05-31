#include "character_panel.h"

#include "config.h"
#include "world.h"

namespace silencer::client_ui::lobby {

namespace character_panel_detail {

constexpr const char * kActionAgents = "lobby.character.agents";

}  // namespace character_panel_detail

void CharacterPanelInit(CharacterPanelState & state) {
	state.selectedAgency = Config::GetInstance().defaultagency;
	state.lastReconciled = -1;
	state.newCharacterRequested = false;
}

void CharacterPanelTick(CharacterPanelState & state, World & world) {
	state.selectedAgency =
		world.lobby.GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
	state.agentSelectionLocked = world.IsConnected();
	if(static_cast<int>(state.selectedAgency) != state.lastReconciled){
		state.lastReconciled = state.selectedAgency;
		if(world.IsConnected()){
			world.SetAgency(state.selectedAgency);
		}
	}
}

bool CharacterPanelHandleUiIntent(CharacterPanelState & state,
                                  World & world,
                                  const silencer::ui::UiAction & action) {
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == character_panel_detail::kActionAgents){
		if(world.IsConnected()) return true;
		state.newCharacterRequested = true;
		return true;
	}
	return false;
}

}  // namespace silencer::client_ui::lobby
