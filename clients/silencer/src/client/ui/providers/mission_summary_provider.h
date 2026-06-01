#pragma once

class ScreenContext;

#include <memory>

namespace silencer {
namespace client_ui {

struct MissionSummaryProviderState;

struct MissionSummaryProviderValue {
	std::shared_ptr<MissionSummaryProviderState> state;
};

MissionSummaryProviderValue MakeMissionSummaryProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
