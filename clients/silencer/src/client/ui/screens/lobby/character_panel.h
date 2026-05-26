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

class ScreenContext;

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::lobby {

struct CharacterPanelState {
	// Currently-selected agency (Team::NOXIS .. Team::BLACKROSE). For
	// authenticated accounts this mirrors the selected character's locked
	// agency; offline fallback keeps using ScreenContext's default agency.
	Uint8 selectedAgency = 0;
	// Last selectedAgency reflected through ScreenContext. -1 forces a
	// first-frame reconcile on entry.
	int lastReconciled = -1;
	bool newCharacterRequested = false;
	bool agentSelectionLocked = false;
};

// Initialise state from ScreenContext's default agency.
void CharacterPanelInit(CharacterPanelState & state, ScreenContext & ctx);

// Reconcile selected agency with lobby runtime state. Called once per
// LobbyScreen::Tick.
void CharacterPanelTick(CharacterPanelState & state, ScreenContext & ctx);
bool CharacterPanelHandleUiIntent(CharacterPanelState & state,
                                  ScreenContext & ctx,
                                  const silencer::ui::UiAction & action);

// Emit the panel subtree. Must be called inside an open Clay layout pass,
// after the UI frame payload arenas have been reset.
void BuildCharacterPanelTree(CharacterPanelState & state,
                             Uint16 panelWidth,
                             silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
