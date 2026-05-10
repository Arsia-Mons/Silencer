#ifndef GAME_CREATE_PANEL_H
#define GAME_CREATE_PANEL_H

#include "panel.h"

class LobbyScreen;

// Right-side game-creation form on the LobbyScreen: security toggle, level
// range, max players/teams, map list (with downloadable [DL] entries),
// game name, password, and the Create button. Owns the per-frame map
// preview tracking, [DL] download badge clicks, and the deferred-create
// kickoff (validate → upload map → ask lobby to create the game).
class GameCreatePanel : public Panel
{
public:
	explicit GameCreatePanel(LobbyScreen & owner);
	void Build(ScreenContext & ctx, Interface * parent) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	LobbyScreen & owner;
};

#endif
