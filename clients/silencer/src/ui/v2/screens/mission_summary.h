#ifndef SILENCER_UI_V2_SCREENS_MISSION_SUMMARY_H
#define SILENCER_UI_V2_SCREENS_MISSION_SUMMARY_H

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct MissionSummaryHandlers {
	std::function<void()> on_done;
};

Node BuildMissionSummary(const Context & ctx, const MissionSummaryHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
