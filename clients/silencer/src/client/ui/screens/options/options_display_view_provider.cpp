#include "client/ui/screens/options/options_display_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext OptionsDisplayContext = {};
const OptionsDisplayContextValue kEmptyOptionsDisplay = {};
}  // namespace

const OptionsDisplayContextValue& UseOptionsDisplay() {
	const auto * value = static_cast<const OptionsDisplayContextValue *>(
		::use_context(&OptionsDisplayContext));
	if(value) return *value;
	::react_report_error("client/ui/options: missing OptionsDisplayProvider for UseOptionsDisplay\n");
	return kEmptyOptionsDisplay;
}

::ui::UiElement OptionsDisplayView(const OptionsDisplayViewProps& props) {
	const OptionsDisplayContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyOptionsDisplay);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"OptionsDisplayProvider",
		&OptionsDisplayContext,
		const_cast<OptionsDisplayContextValue *>(stored),
		::ui::children({
			::ui::component("OptionsDisplayFrame",
			                OptionsDisplayFrameProps{ .key = "frame" },
			                OptionsDisplayFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
