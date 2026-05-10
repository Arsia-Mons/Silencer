#ifndef GAME_CREATE_PANEL_H
#define GAME_CREATE_PANEL_H

#include "panel.h"

#include <SDL3/SDL_stdinc.h>

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
	// Build a floating preview iface (minimap + name + description) from the
	// map at `filename`. Returned id is the new Interface id on world; caller
	// owns the lifetime via DestroyInterface.
	Uint16 BuildMapPreview(class World & world, const char * filename);
	void TearDownMapPreview(class World & world);

	LobbyScreen & owner;
	Uint16 mapPreviewId = 0;
};

#endif
