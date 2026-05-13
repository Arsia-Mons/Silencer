#ifndef LOBBY_CONNECT_SCREEN_H
#define LOBBY_CONNECT_SCREEN_H

#include "screen.h"

#include <string>
#include <vector>

class LobbyConnectScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiAction(ScreenContext & ctx, silencer::ui::UiNavAction action) override;

	void NotifyLoginClicked();
	void NotifyCancelClicked();

private:
	void AppendLog(const char * text);

	// Reset on entry; flipped to true after the first MOTD render so the
	// banner doesn't re-print every tick. Migrated from Game::motdprinted.
	bool motdprinted = false;
	bool loginClicked = false;
	bool cancelClicked = false;
	std::vector<std::string> logLines;
	char username[17] = {};
	char password[29] = {};
};

#endif
