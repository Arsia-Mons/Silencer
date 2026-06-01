#pragma once

class ScreenContext;
class World;

namespace silencer {
namespace client_ui {

struct MissionSummaryProviderValue {
	World * world = nullptr;
};

MissionSummaryProviderValue MakeMissionSummaryProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
