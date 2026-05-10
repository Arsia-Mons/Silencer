#ifndef GAME_JOIN_PANEL_H
#define GAME_JOIN_PANEL_H

#include "panel.h"

class LobbyScreen;

// Right-side join panel on the LobbyScreen: 3 buttons (Ready / Change Team /
// Choose Tech). Built when world.state transitions to CONNECTED inside
// Game::TickLobbyBody (LobbyScreen::ShowGameJoin), and rebuilt when the
// player clicks "Back To Teams" inside the gametech surface. The Ready
// button text flips to "Waiting..." while the host is still waiting for all
// peers to download the map; restored to "Ready" otherwise.
class GameJoinPanel : public Panel
{
public:
	explicit GameJoinPanel(LobbyScreen & owner);
	void Build(ScreenContext & ctx, Interface * parent) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	LobbyScreen & owner;
};

#endif
