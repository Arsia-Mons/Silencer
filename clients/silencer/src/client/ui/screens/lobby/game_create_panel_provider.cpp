#include "client/ui/screens/lobby/game_create_panel.h"

#include "ui/runtime/react.h"

namespace silencer::client_ui::lobby {

namespace {

::ReactContext LobbyGameCreateContext = {};
const LobbyGameCreate kEmptyGameCreate = {};

}  // namespace

const LobbyGameCreate& UseLobbyGameCreate() {
	const auto * value = static_cast<const LobbyGameCreate *>(
		::use_context(&LobbyGameCreateContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyGameCreateProvider for UseLobbyGameCreate\n");
	return kEmptyGameCreate;
}

::ui::UiElement LobbyGameCreateProvider(const LobbyGameCreate& value,
                                        ::ui::UiChildren children,
                                        const char * key) {
	const LobbyGameCreate * stored = ::ui::copy_value(value);
	if(!stored){
		::react_report_error("client/ui/lobby: failed to store LobbyGameCreateProvider value\n");
		return ::ui::empty();
	}
	return ::ui::provider(
		"LobbyGameCreateProvider",
		&LobbyGameCreateContext,
		const_cast<LobbyGameCreate *>(stored),
		children,
		key);
}

}  // namespace silencer::client_ui::lobby
