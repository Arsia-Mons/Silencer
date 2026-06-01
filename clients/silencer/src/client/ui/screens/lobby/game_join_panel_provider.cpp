#include "client/ui/screens/lobby/game_join_panel.h"

#include "ui/runtime/react.h"

namespace silencer::client_ui::lobby {

namespace {

::ReactContext LobbyGameJoinContext = {};
const LobbyGameJoin kEmptyGameJoin = {};

}  // namespace

const LobbyGameJoin& UseLobbyGameJoin() {
	const auto * value = static_cast<const LobbyGameJoin *>(
		::use_context(&LobbyGameJoinContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyGameJoinProvider for UseLobbyGameJoin\n");
	return kEmptyGameJoin;
}

::ui::UiElement LobbyGameJoinProvider(const LobbyGameJoin& value,
                                      ::ui::UiChildren children,
                                      const char * key) {
	const LobbyGameJoin * stored = ::ui::copy_value(value);
	if(!stored){
		::react_report_error("client/ui/lobby: failed to store LobbyGameJoinProvider value\n");
		return ::ui::empty();
	}
	return ::ui::provider(
		"LobbyGameJoinProvider",
		&LobbyGameJoinContext,
		const_cast<LobbyGameJoin *>(stored),
		children,
		key);
}

}  // namespace silencer::client_ui::lobby
