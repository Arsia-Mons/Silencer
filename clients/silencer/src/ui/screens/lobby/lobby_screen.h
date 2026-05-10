#ifndef LOBBY_SCREEN_H
#define LOBBY_SCREEN_H

#include "screen.h"
#include "character_panel.h"
#include "chat_panel.h"
#include <memory>

class GameSelectPanel;

// In-progress migration of the lobby surface out of Game::ProcessLobbyInterface.
// Build delegates to Game::CreateLobbyInterface() (legacy chrome + un-migrated
// panels) and Tick delegates to Game::TickLobbyBody() (legacy lobby pump).
// Migrated panels live as members and run before the legacy pump each frame
// so panel handlers see fresh button-clicked / textinput-enterpressed flags
// before the recursive walk in ProcessLobbyInterface clears them.
class LobbyScreen : public Screen
{
public:
	LobbyScreen();
	~LobbyScreen() override;
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

	// Right-side panel swap helpers. Called by panels (GameSelectPanel's
	// "Create Game" button) and by Game::GoBack when transitioning out of
	// gamecreate / gamejoin / gametech back to gameselect. Stage D handles
	// GameSelect; the others still go through legacy Game::Create*Interface.
	void ShowGameSelect(ScreenContext & ctx);
	void ShowGameCreate(ScreenContext & ctx);

private:
	CharacterPanel character;
	ChatPanel chat;
	std::unique_ptr<GameSelectPanel> gameSelect;
};

#endif
