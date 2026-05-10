#ifndef CHARACTER_PANEL_H
#define CHARACTER_PANEL_H

#include "panel.h"

// Character box on the LobbyScreen: username header, agency toggles, and the
// LEVEL/WINS/LOSSES/XP readouts beneath. Owns the agency-change detection
// state machine that pushes Config::defaultagency + world.SetAgency on toggle
// changes, and the lazy refresh of the four readouts when the user's stats
// arrive from the lobby's user-info reply.
class CharacterPanel : public Panel
{
public:
	void Build(ScreenContext & ctx, Interface * parent) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	// Last agency rendered into the readouts. -1 forces an initial refresh
	// on first Tick after Build (matches the legacy behavior: case LOBBY:
	// stateisnew set agencychanged = true to trigger first-frame display).
	int oldselectedagency = -1;
	bool agencychanged = true;
};

#endif
