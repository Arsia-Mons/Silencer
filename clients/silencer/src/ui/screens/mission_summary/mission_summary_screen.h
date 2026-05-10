#ifndef MISSION_SUMMARY_SCREEN_H
#define MISSION_SUMMARY_SCREEN_H

#include "screen.h"

class TextBox;
class Stats;

// End-of-mission stats screen with optional upgrade buttons. Owns the
// upgrade-availability poll (driven by world.lobby.statupgraded /
// retrieving) plus the Continue button that returns to LOBBY (if
// authenticated) or MAINMENU.
class MissionSummaryScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	void Refresh(ScreenContext & ctx);
	static void AddSummaryLine(TextBox & textbox, const char * name, Uint32 value, bool percentage = false);

	bool infoLoaded = false;
};

#endif
