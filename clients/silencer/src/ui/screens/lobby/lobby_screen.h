#ifndef LOBBY_SCREEN_H
#define LOBBY_SCREEN_H

#include "screen.h"

// Stage-A adapter: Build delegates to Game::CreateLobbyInterface() and Tick
// delegates to Game::TickLobbyBody(). Subsequent stages migrate one panel at
// a time out of those legacy helpers and onto Panel members of LobbyScreen.
class LobbyScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;
};

#endif
