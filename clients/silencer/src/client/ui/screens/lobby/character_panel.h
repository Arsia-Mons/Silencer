#ifndef SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H

// Screen-side lobby CharacterPanel. Composes Text, sprite chrome, and Button
// primitives into the compact lobby identity panel: fitted agent name,
// selected agency emblem, core record stats, and navigation to the
// character selection/create screen.
//
// Domain glue lives behind LobbyProvider/use_lobby. This panel keeps only
// local UI state and renders the model snapshot it receives.

#include "shared.h"
#include "runtime/UiActionQueue.h"

namespace silencer::ui {
class UiInteractionRegistry;
}

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
	bool newCharacterRequested = false;
	bool agentSelectionLocked = false;
};

// Initialise state from the lobby character model default agency.
void CharacterPanelInit(CharacterPanelState & state,
                        LobbyCharacterModel & character);

// Reconcile any selectedAgency change through the lobby character model.
// Called once per LobbyScreen::Tick.
void CharacterPanelTick(CharacterPanelState & state,
                        LobbyCharacterModel & character);
bool CharacterPanelHandleUiIntent(CharacterPanelState & state,
                                  LobbyCharacterModel & character,
                                  const silencer::ui::UiAction & action);

// Emit the panel subtree. Must be called inside an open Clay layout pass,
// after the UI frame payload arenas have been reset.
void BuildCharacterPanelTree(CharacterPanelState & state,
                             Uint16 panelWidth,
                             LobbyCharacterModel & character,
                             silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
