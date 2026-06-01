#include "character_panel.h"

#include "client/ui/hooks/use_lobby.h"

#include <string>

namespace silencer::client_ui::lobby {
namespace character_panel_detail {

void WriteFallbackStats(CharacterPanelState& state) {
	state.levelLabel = "LV 0";
	state.wins = "0";
	state.losses = "0";
	state.xp = "0/100";
	state.endurance = "0";
	state.shield = "0";
	state.jetpack = "0";
	state.techslots = "0";
	state.hacking = "0";
	state.contacts = "0";
}

void RefreshSnapshot(CharacterPanelState& state,
                     LobbyCharacterModel& character) {
	const LobbyCharacterPanelSnapshot snapshot =
		character.panel(state.selectedAgency);
	state.agentSelectionLocked = snapshot.agent_selection_locked;
	state.displayAgency = snapshot.agency;
	state.agentName = snapshot.agent_name;

	if(!snapshot.progress.loaded){
		WriteFallbackStats(state);
		return;
	}

	const LobbyCharacterProgress& stats = snapshot.progress;
	state.levelLabel = "LV " + std::to_string(stats.level);
	state.wins = std::to_string(stats.wins);
	state.losses = std::to_string(stats.losses);
	if(stats.max_level){
		state.xp = "MAX";
	}else{
		const int nextLevelXp = 100 * (static_cast<int>(stats.level) + 1);
		state.xp =
			std::to_string(stats.xptonextlevel) + "/" +
			std::to_string(nextLevelXp);
	}
	state.endurance = std::to_string(stats.endurance);
	state.shield = std::to_string(stats.shield);
	state.jetpack = std::to_string(stats.jetpack);
	state.techslots = std::to_string(stats.techslots);
	state.hacking = std::to_string(stats.hacking);
	state.contacts = std::to_string(stats.contacts);
}

}  // namespace character_panel_detail

void CharacterPanelInit(CharacterPanelState& state,
                        LobbyCharacterModel& character) {
	state = CharacterPanelState{};
	state.selectedAgency = character.default_agency();
	state.displayAgency = state.selectedAgency;
	state.lastReconciled = -1;
	character_panel_detail::RefreshSnapshot(state, character);
}

void CharacterPanelTick(CharacterPanelState& state,
                        LobbyCharacterModel& character) {
	state.selectedAgency = character.selected_agency();
	state.agentSelectionLocked = character.agent_selection_locked();
	if(static_cast<int>(state.selectedAgency) != state.lastReconciled){
		state.lastReconciled = state.selectedAgency;
		character.apply_selected_agency(state.selectedAgency);
	}
	character_panel_detail::RefreshSnapshot(state, character);
}

}  // namespace silencer::client_ui::lobby
