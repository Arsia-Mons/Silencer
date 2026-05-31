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
	state.agentSelectionLocked = false;
	state.agentName = "Agent";
	state.levelText = "No character selected";
	state.recordText.clear();
	state.statsLineA.clear();
	state.statsLineB.clear();
}

void CharacterPanelTick(CharacterPanelState & state, World & world) {
	const auto * ch = world.lobby.GetSelectedCharacter();
	state.selectedAgency = ch
		? ch->agencyIdx
		: world.lobby.GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
	state.agentSelectionLocked = world.IsConnected();
	const char * agentName = ch ? ch->name : world.lobby.GetLocalUsername();
	if(!agentName || agentName[0] == '\0') agentName = "Agent";
	state.agentName = agentName;
	if(ch){
		state.levelText = "Level " + std::to_string(ch->stats.level) +
			" XP " + std::to_string(ch->stats.xp);
		state.recordText = "Wins " + std::to_string(ch->stats.wins) +
			" Losses " + std::to_string(ch->stats.losses);
		state.statsLineA = "End " + std::to_string(ch->stats.endurance) +
			" Shield " + std::to_string(ch->stats.shield) +
			" Jet " + std::to_string(ch->stats.jetpack);
		state.statsLineB = "Tech " + std::to_string(ch->stats.techslots) +
			" Hack " + std::to_string(ch->stats.hacking) +
			" Contacts " + std::to_string(ch->stats.contacts);
	}else{
		state.levelText = "No character selected";
		state.recordText.clear();
		state.statsLineA.clear();
		state.statsLineB.clear();
	}
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
