#ifndef LOBBY_SCREEN_H
#define LOBBY_SCREEN_H

#include "screen.h"
#include "character_panel.h"

// In-progress migration of the lobby surface out of Game::ProcessLobbyInterface.
// Build delegates to Game::CreateLobbyInterface() (legacy chrome + un-migrated
// panels) and Tick delegates to Game::TickLobbyBody() (legacy lobby pump).
// Migrated panels live as members and run after the legacy pump each frame.
class LobbyScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	CharacterPanel character;
};

#endif
