#include "client/ui/screens/lobby/game_select_panel.h"

#include "ui/runtime/react.h"

namespace silencer::client_ui::lobby {

namespace {

::ReactContext LobbyGameSelectContext = {};
const LobbyGameSelect kEmptyGameSelect = {};

}  // namespace

const LobbyGameSelect& UseLobbyGameSelect() {
	const auto * value = static_cast<const LobbyGameSelect *>(
		::use_context(&LobbyGameSelectContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyGameSelectProvider for UseLobbyGameSelect\n");
	return kEmptyGameSelect;
}

::ui::UiElement LobbyGameSelectProvider(const LobbyGameSelect& value,
                                        ::ui::UiChildren children,
                                        const char * key) {
	const LobbyGameSelect * stored = ::ui::copy_value(value);
	if(!stored){
		::react_report_error("client/ui/lobby: failed to store LobbyGameSelectProvider value\n");
		return ::ui::empty();
	}
	return ::ui::provider(
		"LobbyGameSelectProvider",
		&LobbyGameSelectContext,
		const_cast<LobbyGameSelect *>(stored),
		children,
		key);
}

}  // namespace silencer::client_ui::lobby
