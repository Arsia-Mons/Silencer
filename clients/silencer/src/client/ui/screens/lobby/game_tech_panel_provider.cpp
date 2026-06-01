#include "client/ui/screens/lobby/game_tech_panel.h"

#include "ui/runtime/react.h"

namespace silencer::client_ui::lobby {

namespace {

::ReactContext LobbyGameTechContext = {};
const LobbyGameTech kEmptyGameTech = {};

}  // namespace

const LobbyGameTech& UseLobbyGameTech() {
	const auto * value = static_cast<const LobbyGameTech *>(
		::use_context(&LobbyGameTechContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyGameTechProvider for UseLobbyGameTech\n");
	return kEmptyGameTech;
}

::ui::UiElement LobbyGameTechProvider(const LobbyGameTech& value,
                                      ::ui::UiChildren children,
                                      const char * key) {
	const LobbyGameTech * stored = ::ui::copy_value(value);
	if(!stored){
		::react_report_error("client/ui/lobby: failed to store LobbyGameTechProvider value\n");
		return ::ui::empty();
	}
	return ::ui::provider(
		"LobbyGameTechProvider",
		&LobbyGameTechContext,
		const_cast<LobbyGameTech *>(stored),
		children,
		key);
}

}  // namespace silencer::client_ui::lobby
