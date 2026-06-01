#include "client/ui/hud/ingame_hud_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext InGameHudContext = {};
const InGameHudContextValue kEmptyInGameHud = {};
}  // namespace

const InGameHudContextValue& UseInGameHud() {
	const auto * value = static_cast<const InGameHudContextValue *>(
		::use_context(&InGameHudContext));
	return value ? *value : kEmptyInGameHud;
}

::ui::UiElement InGameHudView(const InGameHudViewProps& props) {
	const InGameHudContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyInGameHud);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"InGameHudProvider",
		&InGameHudContext,
		const_cast<InGameHudContextValue *>(stored),
		::ui::children({
			::ui::component("InGameHudFrame",
			                InGameHudFrameProps{ .key = "frame" },
			                InGameHudFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
