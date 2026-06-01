#include "client/ui/screens/lobby/character_panel.h"

#include "ui/runtime/react.h"

namespace silencer::client_ui::lobby {

namespace {

::ReactContext LobbyCharacterContext = {};
const LobbyCharacter kEmptyCharacter = {};

}  // namespace

const LobbyCharacter& UseLobbyCharacter() {
	const auto * value = static_cast<const LobbyCharacter *>(
		::use_context(&LobbyCharacterContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyCharacterProvider for UseLobbyCharacter\n");
	return kEmptyCharacter;
}

::ui::UiElement LobbyCharacterProvider(const LobbyCharacter& value,
                                       ::ui::UiChildren children,
                                       const char * key) {
	const LobbyCharacter * stored = ::ui::copy_value(value);
	if(!stored){
		::react_report_error("client/ui/lobby: failed to store LobbyCharacterProvider value\n");
		return ::ui::empty();
	}
	return ::ui::provider(
		"LobbyCharacterProvider",
		&LobbyCharacterContext,
		const_cast<LobbyCharacter *>(stored),
		children,
		key);
}

}  // namespace silencer::client_ui::lobby
