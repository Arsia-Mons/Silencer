#ifndef MISSION_SUMMARY_SCREEN_H
#define MISSION_SUMMARY_SCREEN_H

#include "client/ui/hooks/use_mission_summary.h"
#include "client/ui/retained/RetainedFrame.h"
#include "screen.h"

// End-of-mission stats screen with optional upgrade buttons. The stat snapshot,
// upgrade request, and continue destination flow through use_mission_summary().
class MissionSummaryScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, const silencer::ui::UiInputState& input, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;
	const ::ui::DrawCommandList * RetainedDrawCommands() const override;

private:
	int MaxScroll() const;
	void ClampScroll();

	bool infoLoaded = false;
	int scrollDelta = 0;
	int scrollPosition = 0;
	silencer::client_ui::MissionSummarySnapshot summary;
	silencer::client_ui::RetainedFrame retainedFrame_;
};

#endif
