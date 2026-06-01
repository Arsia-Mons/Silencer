#ifndef LOBBY_CONNECT_SCREEN_H
#define LOBBY_CONNECT_SCREEN_H

#include "client/ui/retained/RetainedFrame.h"
#include "screen.h"

#include <string>
#include <vector>

class LobbyConnectScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;
	const ::ui::DrawCommandList * RetainedDrawCommands() const override;

private:
	void AppendLog(const char * text);
	void UpdateLogText();

	// Reset on entry; flipped to true after the first MOTD render so the
	// banner doesn't re-print every tick. Migrated from Game::motdprinted.
	bool motdprinted = false;
	std::vector<std::string> logLines;
	char username[17] = {};
	char password[29] = {};
	std::string logText;
	std::string usernameDisplay;
	std::string passwordDisplay;
	silencer::client_ui::RetainedFrame retainedFrame_;
};

#endif
