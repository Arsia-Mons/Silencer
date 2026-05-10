#ifndef LOBBY_CONNECT_SCREEN_H
#define LOBBY_CONNECT_SCREEN_H

#include "screen.h"

class LobbyConnectScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	// Reset on entry; flipped to true after the first MOTD render so the
	// banner doesn't re-print every tick. Migrated from Game::motdprinted.
	bool motdprinted = false;
};

#endif
