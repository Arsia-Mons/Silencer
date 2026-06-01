#pragma once

#include "client/ui/providers/mission_summary_provider.h"

#include <array>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

enum class MissionSummaryDestination {
	MainMenu,
	Lobby,
};

struct MissionSummarySnapshot {
	bool loaded = false;
	bool upgrade_banner = false;
	int experience = 0;
	std::vector<std::string> lines;
	std::array<int, 6> levels = {};
	std::array<bool, 6> upgrades_available = {};
};

class MissionSummaryUpgradesModel {
public:
	explicit MissionSummaryUpgradesModel(const MissionSummaryProviderValue& provider);

	void apply(int index) const;

private:
	MissionSummaryProviderValue provider_;
};

class MissionSummaryModel {
public:
	explicit MissionSummaryModel(const MissionSummaryProviderValue& provider);

	bool needs_refresh(bool info_loaded) const;
	MissionSummarySnapshot refresh() const;
	MissionSummaryDestination finish() const;

	MissionSummaryUpgradesModel upgrades;

private:
	MissionSummaryProviderValue provider_;
};

MissionSummaryModel use_mission_summary(const MissionSummaryProviderValue& provider);

}  // namespace client_ui
}  // namespace silencer
