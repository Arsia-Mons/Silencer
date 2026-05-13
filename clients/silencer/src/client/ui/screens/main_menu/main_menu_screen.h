#ifndef MAIN_MENU_SCREEN_H
#define MAIN_MENU_SCREEN_H

#include "screen.h"

class MainMenuScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleKeyPress(ScreenContext & ctx, char ascii) override;

	void NotifyTutorialClicked() { tutorialClicked = true; }
	void NotifyLobbyClicked() { lobbyClicked = true; }
	void NotifyOptionsClicked() { optionsClicked = true; }
	void NotifyExitClicked() { exitClicked = true; }

private:
	bool tutorialClicked = false;
	bool lobbyClicked = false;
	bool optionsClicked = false;
	bool exitClicked = false;
};

#endif
