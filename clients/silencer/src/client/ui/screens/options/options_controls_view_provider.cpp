#include "client/ui/screens/options/options_controls_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext OptionsControlsContext = {};
const OptionsControls kEmptyOptionsControls = {};
}  // namespace

const OptionsControls& UseOptionsControls() {
	const auto * value = static_cast<const OptionsControls *>(
		::use_context(&OptionsControlsContext));
	if(value) return *value;
	::react_report_error("client/ui/options: missing OptionsControlsProvider for UseOptionsControls\n");
	return kEmptyOptionsControls;
}

::ui::UiElement OptionsControlsView(const OptionsControlsViewProps& props) {
	const OptionsControls * stored = ::ui::copy_value(
		props.controls ? *props.controls : kEmptyOptionsControls);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"OptionsControlsProvider",
		&OptionsControlsContext,
		const_cast<OptionsControls *>(stored),
		::ui::children({
			::ui::component("OptionsControlsFrame",
			                OptionsControlsFrameProps{ .key = "view" },
			                OptionsControlsFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
