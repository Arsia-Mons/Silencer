#include "client/ui/screens/update/update_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext UpdateContext = {};
const UpdateStatus kEmptyUpdate = {};
}  // namespace

const UpdateStatus& UseUpdateStatus() {
	const auto * value = static_cast<const UpdateStatus *>(
		::use_context(&UpdateContext));
	if(value) return *value;
	::react_report_error("client/ui/update: missing UpdateProvider for UseUpdateStatus\n");
	return kEmptyUpdate;
}

::ui::UiElement UpdateView(const UpdateViewProps& props) {
	const UpdateStatus * stored = ::ui::copy_value(
		props.status ? *props.status : kEmptyUpdate);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"UpdateProvider",
		&UpdateContext,
		const_cast<UpdateStatus *>(stored),
		::ui::children({
			::ui::component("UpdateFrame",
			                UpdateFrameProps{ .key = "frame" },
			                UpdateFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
