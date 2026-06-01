#include "client/ui/hud/ingame_hud_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext InGameHudContext = {};
const InGameHud kEmptyInGameHud = {};
}  // namespace

const InGameHud& UseInGameHud() {
	const auto * value = static_cast<const InGameHud *>(
		::use_context(&InGameHudContext));
	if(value) return *value;
	::react_report_error("client/ui/hud: missing InGameHudProvider for UseInGameHud\n");
	return kEmptyInGameHud;
}

::ui::UiElement InGameHudView(const InGameHudViewProps& props) {
	const InGameHud * stored = ::ui::copy_value(
		props.hud ? *props.hud : kEmptyInGameHud);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"InGameHudProvider",
		&InGameHudContext,
		const_cast<InGameHud *>(stored),
		::ui::children({
			::ui::component("InGameHudFrame",
			                InGameHudFrameProps{ .key = "frame" },
			                InGameHudFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
