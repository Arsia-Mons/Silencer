#include "client/ui/screens/options/options_display_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext OptionsDisplayContext = {};
const OptionsDisplay kEmptyOptionsDisplay = {};
}  // namespace

const OptionsDisplay& UseOptionsDisplay() {
	const auto * value = static_cast<const OptionsDisplay *>(
		::use_context(&OptionsDisplayContext));
	if(value) return *value;
	::react_report_error("client/ui/options: missing OptionsDisplayProvider for UseOptionsDisplay\n");
	return kEmptyOptionsDisplay;
}

::ui::UiElement OptionsDisplayView(const OptionsDisplayViewProps& props) {
	const OptionsDisplay * stored = ::ui::copy_value(
		props.display ? *props.display : kEmptyOptionsDisplay);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"OptionsDisplayProvider",
		&OptionsDisplayContext,
		const_cast<OptionsDisplay *>(stored),
		::ui::children({
			::ui::component("OptionsDisplayFrame",
			                OptionsDisplayFrameProps{ .key = "frame" },
			                OptionsDisplayFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
