#include "client/ui/screens/lobby/chat_panel.h"

#include "ui/runtime/react.h"

namespace silencer::client_ui::lobby {

namespace {

::ReactContext LobbyChatContext = {};
const LobbyChat kEmptyChat = {};

}  // namespace

const LobbyChat& UseLobbyChat() {
	const auto * value = static_cast<const LobbyChat *>(
		::use_context(&LobbyChatContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyChatProvider for UseLobbyChat\n");
	return kEmptyChat;
}

::ui::UiElement LobbyChatProvider(const LobbyChat& value,
                                  ::ui::UiChildren children,
                                  const char * key) {
	const LobbyChat * stored = ::ui::copy_value(value);
	if(!stored){
		::react_report_error("client/ui/lobby: failed to store LobbyChatProvider value\n");
		return ::ui::empty();
	}
	return ::ui::provider(
		"LobbyChatProvider",
		&LobbyChatContext,
		const_cast<LobbyChat *>(stored),
		children,
		key);
}

}  // namespace silencer::client_ui::lobby
