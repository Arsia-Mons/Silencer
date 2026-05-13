#ifndef SILENCER_UI_LOBBY_CLAY_CHARACTER_PANEL_H
#define SILENCER_UI_LOBBY_CLAY_CHARACTER_PANEL_H

// Screen-side Clay reimplementation of the lobby CharacterPanel. Composes
// the Toggle + BankText primitives into the legacy character box layout
// (username header, 5 agency toggles, LEVEL / WINS / LOSSES / XP readouts).
//
// Domain glue (Config::Save, World::SetAgency, agency-change detection)
// lives HERE in the screen — not in the primitives. The primitives stay
// screen-agnostic.

#include "shared.h"

class World;
class Resources;

namespace silencer::ui::lobby_clay {

struct CharacterPanelState {
	// Currently-selected agency (Team::NOXIS .. Team::BLACKROSE). Mirrors
	// Config::defaultagency; toggle onClicks mutate this field, and Tick
	// reconciles into Config + world.SetAgency on change.
	Uint8 selectedAgency = 0;
	// Last selectedAgency reflected into Config + world. -1 forces a first-
	// frame persist on entry (matches legacy's initial agencychanged=true).
	int lastReconciled = -1;
};

// Initialise state from Config (defaultagency).
void CharacterPanelInit(CharacterPanelState & state);

// Reconcile any selectedAgency change with Config + world. Called once per
// LobbyClayScreen::Tick.
void CharacterPanelTick(CharacterPanelState & state, World & world);

// Emit the Clay subtree. Must be called inside an open Clay layout pass,
// after BankTextBeginFrame() + ToggleBeginFrame() have been invoked.
void BuildCharacterPanelTree(CharacterPanelState & state,
                             World & world,
                             Resources & resources);

}  // namespace silencer::ui::lobby_clay

#endif
