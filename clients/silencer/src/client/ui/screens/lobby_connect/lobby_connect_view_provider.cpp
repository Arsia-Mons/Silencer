#include "client/ui/screens/lobby_connect/lobby_connect_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext LobbyConnectContext = {};
const LobbyConnectContextValue kEmptyLobbyConnect = {};
}  // namespace

const LobbyConnectContextValue& UseLobbyConnect() {
	const auto * value = static_cast<const LobbyConnectContextValue *>(
		::use_context(&LobbyConnectContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby-connect: missing LobbyConnectProvider for UseLobbyConnect\n");
	return kEmptyLobbyConnect;
}

::ui::UiElement LobbyConnectView(const LobbyConnectViewProps& props) {
	const LobbyConnectContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyLobbyConnect);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"LobbyConnectProvider",
		&LobbyConnectContext,
		const_cast<LobbyConnectContextValue *>(stored),
		::ui::children({
			::ui::component("LobbyConnectFrame",
			                LobbyConnectFrameProps{ .key = "frame" },
			                LobbyConnectFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
