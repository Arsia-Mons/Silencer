#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <array>
#include <functional>

namespace silencer::client_ui {

constexpr int kMissionSummaryVisibleLines = 27;
constexpr int kMissionSummaryUpgradeCount = 6;

enum class MissionSummaryDestination {
	MainMenu,
	Lobby,
};

struct MissionSummary {
	const char * xp = "";
	bool upgrade_banner = false;
	std::array<const char *, kMissionSummaryVisibleLines> summary_lines = {};
	std::array<int, kMissionSummaryUpgradeCount> levels = {};
	std::array<bool, kMissionSummaryUpgradeCount> upgrades_available = {};
	std::function<void(int)> upgrade = {};
	std::function<MissionSummaryDestination()> done = {};
};

const MissionSummary& UseMissionSummary();

struct MissionSummaryFrameProps {
	const char * key = nullptr;
};

::ui::UiElement MissionSummaryFrame(const MissionSummaryFrameProps& props);

struct MissionSummaryViewProps {
	const char * key = nullptr;
	const MissionSummary * summary = nullptr;
};

::ui::UiElement MissionSummaryView(const MissionSummaryViewProps& props);

}  // namespace silencer::client_ui
