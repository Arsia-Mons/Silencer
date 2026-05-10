#ifndef GAME_SELECT_PANEL_H
#define GAME_SELECT_PANEL_H

#include "panel.h"

class LobbyScreen;

// Right-side server browser on the LobbyScreen: list of active games + the
// per-game info readouts (name, map, security/password, creator, level
// limits) plus the Join Game / Create Game buttons. Owns the legacy
// SELECTBOX uid 10 refresh logic and the Join button flow (level checks,
// password dialog, JoinGame). The Create Game button asks the owning
// LobbyScreen to swap to GameCreate.
class GameSelectPanel : public Panel
{
public:
	explicit GameSelectPanel(LobbyScreen & owner);
	void Build(ScreenContext & ctx, Interface * parent) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	LobbyScreen & owner;
};

#endif
