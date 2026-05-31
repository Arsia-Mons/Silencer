#pragma once

#include "ui/components/common.h"

#include <array>
#include <functional>

namespace silencer::client_ui {

constexpr int kMissionSummaryVisibleLines = 27;
constexpr int kMissionSummaryUpgradeCount = 6;

struct MissionSummaryViewProps {
	const char * key = nullptr;
	const char * xp = "";
	bool upgrade_banner = false;
	std::array<const char *, kMissionSummaryVisibleLines> summary_lines = {};
	std::array<int, kMissionSummaryUpgradeCount> levels = {};
	std::array<bool, kMissionSummaryUpgradeCount> upgrades_available = {};
	std::array<std::function<void(const ::ui::ActivationEvent&)>, kMissionSummaryUpgradeCount> on_upgrade = {};
	std::function<void(const ::ui::ActivationEvent&)> on_done = {};
};

::ui::UiElement MissionSummaryView(const MissionSummaryViewProps& props);

}  // namespace silencer::client_ui
