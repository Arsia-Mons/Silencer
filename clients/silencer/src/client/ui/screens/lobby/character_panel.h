#ifndef SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H

// Screen-side lobby CharacterPanel. Composes Text, sprite chrome, and Button
// primitives into the compact lobby identity panel: fitted agent name,
// selected agency emblem, core record stats, and navigation to the
// character selection/create screen.
//
// Domain reads used during Clay declaration come through UseLobby; deferred
// lobby writes are flushed by the lobby screen lifecycle, outside declaration.

#include "shared.h"
#include "runtime/UiActionQueue.h"

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::lobby {

struct CharacterPanelState {
	bool openCharacterSelectionRequested = false;
};

// Initialise panel-local pending intent state. Live lobby values come from
// UseLobby during Clay declaration.
void CharacterPanelInit(CharacterPanelState & state);

bool CharacterPanelHandleUiIntent(CharacterPanelState & state,
                                  const silencer::ui::UiAction & action);

// Emit the panel subtree. Must be called inside an open Clay layout pass,
// after the UI frame payload arenas have been reset.
void BuildCharacterPanelTree(Uint16 panelWidth,
                             silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
