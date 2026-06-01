#ifndef SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H

// Screen-side lobby CharacterPanel state and domain glue. The cppx lobby view
// owns retained composition; this file owns agency-change reconciliation and
// navigation to the character selection/create screen.

#include "shared.h"
#include "ui/runtime/element.h"

#include <functional>
#include <string>

class World;

namespace silencer::client_ui::lobby {

struct CharacterPanelState {
	// Currently-selected agency (Team::NOXIS .. Team::BLACKROSE). For
	// authenticated accounts this mirrors the selected character's locked
	// agency; offline fallback keeps using Config::defaultagency.
	Uint8 selectedAgency = 0;
	// Last selectedAgency reflected into the world. -1 forces a first-frame
	// reconcile on entry (matches legacy's initial agencychanged=true).
	int lastReconciled = -1;
	bool newCharacterRequested = false;
	bool agentSelectionLocked = false;

	// Cached character snapshot for UseLobbyCharacter() consumers. Rebuilt
	// from World/Lobby during CharacterPanelTick so components do not receive
	// a raw World pointer.
	std::string agentName = "Agent";
	std::string levelText = "No character selected";
	std::string recordText;
	std::string statsLineA;
	std::string statsLineB;
};

struct LobbyCharacter {
	const CharacterPanelState * snapshot = nullptr;
	std::function<void()> change_agent = {};
};

const LobbyCharacter& UseLobbyCharacter();
::ui::UiElement LobbyCharacterProvider(const LobbyCharacter& value,
                                       ::ui::UiChildren children,
                                       const char * key = nullptr);

// Initialise state from Config (defaultagency).
void CharacterPanelInit(CharacterPanelState & state);

// Reconcile any selectedAgency change with Config + world. Called once per
// LobbyScreen::Tick.
void CharacterPanelTick(CharacterPanelState & state, World & world);

}  // namespace silencer::client_ui::lobby

#endif
