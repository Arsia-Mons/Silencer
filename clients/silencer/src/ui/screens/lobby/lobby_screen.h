#ifndef LOBBY_SCREEN_H
#define LOBBY_SCREEN_H

#include "screen.h"
#include "character_panel.h"
#include "chat_panel.h"
#include <memory>

class GameSelectPanel;
class GameCreatePanel;
class GameJoinPanel;

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
	// "Create Game" button, GameJoinPanel's "Choose Tech") and by
	// Game::GoBack and Game::TickLobbyBody when transitioning between
	// gameselect / gamecreate / gamejoin / gametech.
	// ShowGameTech still builds the legacy gametechinterface; Stage G replaces
	// the body with a GameTechPanel construction.
	void ShowGameSelect(ScreenContext & ctx);
	void ShowGameCreate(ScreenContext & ctx);
	void ShowGameJoin(ScreenContext & ctx);
	void ShowGameTech(ScreenContext & ctx);

private:
	CharacterPanel character;
	ChatPanel chat;
	// One of gameSelect / gameCreate / gameJoin is active at a time
	// (right-side panel). Stage G adds GameTechPanel to this set.
	std::unique_ptr<GameSelectPanel> gameSelect;
	std::unique_ptr<GameCreatePanel> gameCreate;
	std::unique_ptr<GameJoinPanel>   gameJoin;
};

#endif
