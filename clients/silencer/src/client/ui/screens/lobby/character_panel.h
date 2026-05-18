#ifndef SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_CHARACTER_PANEL_H

// Screen-side lobby CharacterPanel. Composes the Toggle + Text primitives
// into the legacy character box layout (username header, 5 agency toggles,
// LEVEL / WINS / LOSSES / XP readouts).
//
// Domain glue (Config::Save, World::SetAgency, agency-change detection)
// lives HERE in the screen — not in the primitives. The primitives stay
// screen-agnostic.

#include "shared.h"
#include "runtime/UiActionQueue.h"

class World;
class Resources;

namespace silencer::ui {
class UiInteractionRegistry;
}

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
};

// Initialise state from Config (defaultagency).
void CharacterPanelInit(CharacterPanelState & state);

// Reconcile any selectedAgency change with Config + world. Called once per
// LobbyScreen::Tick.
void CharacterPanelTick(CharacterPanelState & state, World & world);
bool CharacterPanelHandleUiIntent(CharacterPanelState & state,
                                  const silencer::ui::UiAction & action);

// Emit the panel subtree. Must be called inside an open Clay layout pass,
// after TextBeginFrame() + ToggleBeginFrame() have been invoked.
void BuildCharacterPanelTree(CharacterPanelState & state,
                             World & world,
                             Resources & resources,
                             silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::lobby

#endif
