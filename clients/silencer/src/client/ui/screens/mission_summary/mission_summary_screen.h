#ifndef MISSION_SUMMARY_SCREEN_H
#define MISSION_SUMMARY_SCREEN_H

#include "screen.h"

#include <array>
#include <string>
#include <vector>

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
	bool BuildElement(ScreenContext & ctx, ::ui::UiElement * out) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	static constexpr int kVisibleSummaryLineCount = 27;

	void Refresh(ScreenContext & ctx);
	void AddSummaryLine(const char * name, Uint32 value, bool percentage = false);

	bool infoLoaded = false;
	bool doneClicked = false;
	int upgradeClicked = -1;
	int scrollDelta = 0;
	int scrollPosition = 0;
	bool upgradeBanner = false;
	int experience = 0;
	std::string xpText;
	std::array<std::string, kVisibleSummaryLineCount> visibleSummaryLines = {};
	std::vector<std::string> summaryLines;
	std::array<int, 6> levels = {};
	std::array<bool, 6> upgradesAvailable = {};
};

#endif
