#pragma once

#include "ui/runtime/element.h"

#include <array>

namespace silencer {
namespace client_ui {

constexpr int kMissionSummaryVisibleLineCount = 28;
constexpr int kMissionSummaryUpgradeCount = 6;

struct MissionSummaryFrameProps {
	const char * key = nullptr;
	const char * const * summary_lines = nullptr;
	int summary_line_count = 0;
	int experience = 0;
	bool upgrade_banner = false;
	std::array<int, kMissionSummaryUpgradeCount> levels = {};
	std::array<bool, kMissionSummaryUpgradeCount> upgrades_available = {};
};

::ui::UiElement MissionSummaryFrame(const MissionSummaryFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
