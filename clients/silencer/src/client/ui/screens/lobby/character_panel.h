#ifndef SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H

// Lobby character panel state. The retained chrome component composes Text,
// sprite chrome, and Button primitives into the compact lobby identity panel:
// fitted agent name, selected agency emblem, core record stats, and navigation
// to the character selection/create screen.
//
// Domain glue lives behind LobbyProvider/use_lobby. This panel keeps only
// local UI state and renders the model snapshot it receives.

#include "shared.h"
#include <string>

namespace silencer::client_ui {
class LobbyCharacterModel;
}

namespace silencer::client_ui::lobby {

struct CharacterPanelState {
	// Currently-selected agency id. For authenticated accounts this mirrors
	// the selected character's locked agency; offline fallback comes from the
	// lobby character model.
	Uint8 selectedAgency = 0;
	// Last selectedAgency reflected into the world. -1 forces a first-frame
	// reconcile on entry (matches legacy's initial agencychanged=true).
	int lastReconciled = -1;
	bool agentSelectionLocked = false;
	Uint8 displayAgency = 0;
	std::string agentName = "No Agent";
	std::string levelLabel = "LV 0";
	std::string wins = "0";
	std::string losses = "0";
	std::string xp = "0/100";
	std::string endurance = "0";
	std::string shield = "0";
	std::string jetpack = "0";
	std::string techslots = "0";
	std::string hacking = "0";
	std::string contacts = "0";
};

// Initialise state from the lobby character model default agency.
void CharacterPanelInit(CharacterPanelState & state,
                        LobbyCharacterModel & character);

// Reconcile any selectedAgency change through the lobby character model.
// Called once per LobbyScreen::Tick.
void CharacterPanelTick(CharacterPanelState & state,
                        LobbyCharacterModel & character);

}  // namespace silencer::client_ui::lobby

#endif
