#include "client/ui/screens/update/update_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext UpdateContext = {};
const UpdateContextValue kEmptyUpdate = {};
}  // namespace

const UpdateContextValue& UseUpdate() {
	const auto * value = static_cast<const UpdateContextValue *>(
		::use_context(&UpdateContext));
	if(value) return *value;
	::react_report_error("client/ui/update: missing UpdateProvider for UseUpdate\n");
	return kEmptyUpdate;
}

::ui::UiElement UpdateView(const UpdateViewProps& props) {
	const UpdateContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyUpdate);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"UpdateProvider",
		&UpdateContext,
		const_cast<UpdateContextValue *>(stored),
		::ui::children({
			::ui::component("UpdateFrame",
			                UpdateFrameProps{ .key = "frame" },
			                UpdateFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
