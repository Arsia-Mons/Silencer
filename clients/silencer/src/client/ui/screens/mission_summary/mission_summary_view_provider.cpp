#include "client/ui/screens/mission_summary/mission_summary_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext MissionSummaryContext = {};
const MissionSummaryContextValue kEmptyMissionSummary = {};
}  // namespace

const MissionSummaryContextValue& UseMissionSummary() {
	const auto * value = static_cast<const MissionSummaryContextValue *>(
		::use_context(&MissionSummaryContext));
	return value ? *value : kEmptyMissionSummary;
}

::ui::UiElement MissionSummaryView(const MissionSummaryViewProps& props) {
	const MissionSummaryContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyMissionSummary);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"MissionSummaryProvider",
		&MissionSummaryContext,
		const_cast<MissionSummaryContextValue *>(stored),
		::ui::children({
			::ui::component("MissionSummaryFrame",
			                MissionSummaryFrameProps{ .key = "frame" },
			                MissionSummaryFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
