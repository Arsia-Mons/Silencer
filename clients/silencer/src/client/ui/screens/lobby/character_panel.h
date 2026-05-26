#ifndef SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H

// Screen-side lobby CharacterPanel. Composes Text, sprite chrome, and Button
// primitives into the compact lobby identity panel: fitted agent name,
// selected agency emblem, core record stats, and navigation to the
// character selection/create screen.
//
// Domain decisions live here in the screen. Domain reads used during Clay
// declaration come through UseLobby; primitives stay screen-agnostic.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <functional>

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::lobby {

struct CharacterPanelState {
	// Currently-selected agency (Team::NOXIS .. Team::BLACKROSE). For
	// authenticated accounts this mirrors the selected character's locked
	// agency; offline fallback keeps using the configured default agency.
	Uint8 selectedAgency = 0;
	// Last selectedAgency reflected through the lobby hook's deferred sync.
	// -1 forces a first-frame reconcile on entry.
	int lastReconciled = -1;
	bool agentSelectionLocked = false;
	std::function<void(Uint8 agency)> syncSelectedAgency = {};
	std::function<void()> openCharacterSelection = {};
};

// Initialise screen-local callback/cache state. Live lobby values come from
// UseLobby during Clay declaration.
void CharacterPanelInit(CharacterPanelState & state);

bool CharacterPanelHandleUiIntent(CharacterPanelState & state,
                                  const silencer::ui::UiAction & action);

// Emit the panel subtree. Must be called inside an open Clay layout pass,
// after the UI frame payload arenas have been reset.
void BuildCharacterPanelTree(CharacterPanelState & state,
                             Uint16 panelWidth,
                             silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
