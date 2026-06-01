#include "client/ui/screens/options/options_controls_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext OptionsControlsContext = {};
const OptionsControlsContextValue kEmptyOptionsControls = {};
}  // namespace

const OptionsControlsContextValue& UseOptionsControls() {
	const auto * value = static_cast<const OptionsControlsContextValue *>(
		::use_context(&OptionsControlsContext));
	return value ? *value : kEmptyOptionsControls;
}

::ui::UiElement OptionsControlsView(const OptionsControlsViewProps& props) {
	const OptionsControlsContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyOptionsControls);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"OptionsControlsProvider",
		&OptionsControlsContext,
		const_cast<OptionsControlsContextValue *>(stored),
		::ui::children({
			::ui::component("OptionsControlsFrame",
			                OptionsControlsFrameProps{ .key = "view" },
			                OptionsControlsFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
