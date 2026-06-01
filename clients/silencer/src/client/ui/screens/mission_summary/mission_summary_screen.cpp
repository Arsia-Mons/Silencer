#include "mission_summary_screen.h"

#include "client/ui/hooks/use_mission_summary.h"
#include "client/ui/hooks/use_navigation.h"
#include "client/ui/screens/mission_summary/mission_summary_frame.h"
#include "lobby_screen.h"
#include "main_menu_screen.h"
#include "screen_context.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "renderer.h"
#include "surface.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

namespace mission_summary_screen_detail
{
constexpr uint16_t kSummaryH = 300;
constexpr uint8_t kLineH = 11;
constexpr const char * kActionDone = "mission_summary.done";
constexpr const char * kActionUpgradePrefix = "mission_summary.upgrade.";

bool StartsWith(const std::string & value, const char * prefix)
{
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

int SuffixInt(const std::string & value, const char * prefix)
{
	if(!StartsWith(value, prefix)) return -1;
	return std::atoi(value.c_str() + std::strlen(prefix));
}

void FillVisibleSummaryLines(
	const silencer::client_ui::MissionSummarySnapshot& summary,
	int scrollPosition,
	std::array<std::string, silencer::client_ui::kMissionSummaryVisibleLineCount>& out)
{
	for(std::string& line : out){
		line.clear();
	}
	const int lineCount = static_cast<int>(summary.lines.size());
	const int maxStart = std::max(
		0, lineCount - silencer::client_ui::kMissionSummaryVisibleLineCount);
	const int start = std::max(0, std::min(maxStart, scrollPosition));
	for(int i = 0; i < silencer::client_ui::kMissionSummaryVisibleLineCount; i++){
		const int source = start + i;
		if(source >= lineCount) break;
		out[i] = summary.lines[source];
	}
}

} // namespace mission_summary_screen_detail

void MissionSummaryScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	infoLoaded = false;
	doneClicked = false;
	upgradeClicked = -1;
	scrollDelta = 0;
	scrollPosition = 0;
	summary = silencer::client_ui::use_mission_summary(
		silencer::client_ui::MakeMissionSummaryProvider(ctx)).refresh();
	infoLoaded = summary.loaded;
	ClampScroll();
}

void MissionSummaryScreen::Tick(ScreenContext & ctx)
{
	if(scrollDelta != 0){
		scrollPosition = std::max(0, std::min(MaxScroll(), scrollPosition + scrollDelta));
		scrollDelta = 0;
	}
	silencer::client_ui::MissionSummaryModel mission =
		silencer::client_ui::use_mission_summary(
			silencer::client_ui::MakeMissionSummaryProvider(ctx));
	if(mission.needs_refresh(infoLoaded)){
		silencer::client_ui::MissionSummarySnapshot refreshed = mission.refresh();
		if(refreshed.loaded){
			summary = std::move(refreshed);
			infoLoaded = true;
			ClampScroll();
		}
	}
	if(upgradeClicked >= 0){
		int idx = upgradeClicked;
		upgradeClicked = -1;
		mission.upgrades.apply(idx);
	}
	if(doneClicked){
		doneClicked = false;
		silencer::client_ui::MissionSummaryDestination destination =
			mission.finish();
		if(destination == silencer::client_ui::MissionSummaryDestination::Lobby){
			silencer::client_ui::use_navigation()
				.reset_to(std::make_unique<LobbyScreen>());
		}else{
			silencer::client_ui::use_navigation()
				.reset_to(std::make_unique<MainMenuScreen>());
		}
	}
}

void MissionSummaryScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)ctx;
	(void)frametime;
	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));

	std::array<std::string, silencer::client_ui::kMissionSummaryVisibleLineCount>
		visibleLines;
	mission_summary_screen_detail::FillVisibleSummaryLines(
		summary, scrollPosition, visibleLines);
	std::array<const char *, silencer::client_ui::kMissionSummaryVisibleLineCount>
		linePtrs;
	for(int i = 0; i < silencer::client_ui::kMissionSummaryVisibleLineCount; i++){
		linePtrs[i] = visibleLines[i].c_str();
	}

	silencer::client_ui::MissionSummaryFrameProps props{
		.key = "mission-summary",
		.summary_lines = linePtrs.data(),
		.summary_line_count = silencer::client_ui::kMissionSummaryVisibleLineCount,
		.experience = summary.experience,
		.upgrade_banner = summary.upgrade_banner,
		.levels = summary.levels,
		.upgrades_available = summary.upgrades_available,
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::MissionSummaryFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);
}

void MissionSummaryScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MissionSummaryScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::Cancel ||
	   (action.kind == silencer::ui::UiActionKind::Activate && action.id == mission_summary_screen_detail::kActionDone)){
		doneClicked = true;
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Scroll){
		scrollDelta += action.amount;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	int upgrade = mission_summary_screen_detail::SuffixInt(action.id, mission_summary_screen_detail::kActionUpgradePrefix);
	if(upgrade >= 0 && upgrade < 6){
		upgradeClicked = upgrade;
		return true;
	}
	return false;
}

int MissionSummaryScreen::MaxScroll() const
{
	int maxScroll = static_cast<int>(summary.lines.size())
		- (mission_summary_screen_detail::kSummaryH / mission_summary_screen_detail::kLineH);
	return maxScroll < 0 ? 0 : maxScroll;
}

void MissionSummaryScreen::ClampScroll()
{
	scrollPosition = std::max(0, std::min(MaxScroll(), scrollPosition));
}

const ::ui::DrawCommandList * MissionSummaryScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
