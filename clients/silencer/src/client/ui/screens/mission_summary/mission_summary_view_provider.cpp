#include "client/ui/screens/mission_summary/mission_summary_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext MissionSummaryContext = {};
const MissionSummary kEmptyMissionSummary = {};
}  // namespace

const MissionSummary& UseMissionSummary() {
	const auto * value = static_cast<const MissionSummary *>(
		::use_context(&MissionSummaryContext));
	if(value) return *value;
	::react_report_error("client/ui/mission-summary: missing MissionSummaryProvider for UseMissionSummary\n");
	return kEmptyMissionSummary;
}

::ui::UiElement MissionSummaryView(const MissionSummaryViewProps& props) {
	const MissionSummary * stored = ::ui::copy_value(
		props.summary ? *props.summary : kEmptyMissionSummary);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"MissionSummaryProvider",
		&MissionSummaryContext,
		const_cast<MissionSummary *>(stored),
		::ui::children({
			::ui::component("MissionSummaryFrame",
			                MissionSummaryFrameProps{ .key = "frame" },
			                MissionSummaryFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
