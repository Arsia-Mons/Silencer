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

struct MissionSummaryState {
	const char * xp = "";
	bool upgrade_banner = false;
	std::array<const char *, kMissionSummaryVisibleLines> summary_lines = {};
	std::array<int, kMissionSummaryUpgradeCount> levels = {};
	std::array<bool, kMissionSummaryUpgradeCount> upgrades_available = {};
};

struct MissionSummaryActions {
	std::function<void(int)> upgrade = {};
	std::function<MissionSummaryDestination()> done = {};
};

struct MissionSummaryContextValue {
	MissionSummaryState state = {};
	MissionSummaryActions actions = {};
};

const MissionSummaryContextValue& UseMissionSummary();

struct MissionSummaryViewProps {
	const char * key = nullptr;
	const MissionSummaryContextValue * value = nullptr;
};

::ui::UiElement MissionSummaryView(const MissionSummaryViewProps& props);

}  // namespace silencer::client_ui
