#ifndef LOBBY_SCREEN_H
#define LOBBY_SCREEN_H

#include "screen.h"
#include "character_panel.h"
#include "chat_panel.h"
#include <memory>

class GameSelectPanel;
class GameCreatePanel;
class GameJoinPanel;
class GameTechPanel;

// Top-level lobby surface. Owns the lobby chrome (background, title, version,
// map-name overlay, Go Back button), the always-on character + chat panels,
// and one of four mutually-exclusive right-side panels (game select / create
// / join / tech). Drives the lobby pump: deferred CreateGame state machine,
// CONNECTED → GameJoin handoff, disconnect detection, async map-download
// progress, and the create-game progress / error modals.
class LobbyScreen : public Screen
{
public:
	LobbyScreen();
	~LobbyScreen() override;
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleBack(ScreenContext & ctx) override;

	// Right-side panel swap helpers. Called by panels (GameSelectPanel's
	// "Create Game" button, GameJoinPanel's "Choose Tech", GameTechPanel's
	// "Back To Teams") and by Game::GoBack when transitioning between
	// gameselect / gamecreate / gamejoin / gametech.
	void ShowGameSelect(ScreenContext & ctx);
	void ShowGameCreate(ScreenContext & ctx);
	void ShowGameJoin(ScreenContext & ctx);
	void ShowGameTech(ScreenContext & ctx);

	// Update the map-name overlay (uid 8) on the lobby chrome — called by the
	// CONNECTED→GameJoin handoff and by Game::GoBack when leaving a game.
	void SetMapNameOverlay(class World & world, const char * name);

private:
	CharacterPanel character;
	ChatPanel chat;
	// One of gameSelect / gameCreate / gameJoin / gameTech is active at a
	// time (right-side panel).
	std::unique_ptr<GameSelectPanel> gameSelect;
	std::unique_ptr<GameCreatePanel> gameCreate;
	std::unique_ptr<GameJoinPanel>   gameJoin;
	std::unique_ptr<GameTechPanel>   gameTech;
};

#endif
