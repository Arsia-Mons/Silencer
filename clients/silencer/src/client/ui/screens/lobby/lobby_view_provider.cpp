#include "client/ui/screens/lobby/lobby_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {
namespace lobby {

namespace {
::ReactContext LobbyContext = {};
const LobbyContextValue kEmptyLobby = {};
}  // namespace

const LobbyContextValue& UseLobby() {
	const auto * value = static_cast<const LobbyContextValue *>(
		::use_context(&LobbyContext));
	return value ? *value : kEmptyLobby;
}

::ui::UiElement LobbyScreenView(const LobbyScreenViewProps& props) {
	const LobbyContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyLobby);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"LobbyProvider",
		&LobbyContext,
		const_cast<LobbyContextValue *>(stored),
		::ui::children({
			::ui::component("LobbyFrame",
			                LobbyFrameProps{ .key = "view" },
			                LobbyFrame),
		}),
		props.key);
}

}  // namespace lobby
}  // namespace client_ui
}  // namespace silencer
