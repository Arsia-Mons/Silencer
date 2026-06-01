#include "client/ui/screens/lobby_connect/lobby_connect_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext LobbyConnectLogContext = {};
::ReactContext LobbyConnectCredentialsContext = {};
const LobbyConnectLog kEmptyLobbyConnectLog = {};
const LobbyConnectCredentials kEmptyLobbyConnectCredentials = {};
}  // namespace

const LobbyConnectLog& UseLobbyConnectLog() {
	const auto * value = static_cast<const LobbyConnectLog *>(
		::use_context(&LobbyConnectLogContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby-connect: missing LobbyConnectLogProvider for UseLobbyConnectLog\n");
	return kEmptyLobbyConnectLog;
}

const LobbyConnectCredentials& UseLobbyConnectCredentials() {
	const auto * value = static_cast<const LobbyConnectCredentials *>(
		::use_context(&LobbyConnectCredentialsContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby-connect: missing LobbyConnectCredentialsProvider for UseLobbyConnectCredentials\n");
	return kEmptyLobbyConnectCredentials;
}

::ui::UiElement LobbyConnectView(const LobbyConnectViewProps& props) {
	const LobbyConnectLog * log = ::ui::copy_value(
		props.log ? *props.log : kEmptyLobbyConnectLog);
	const LobbyConnectCredentials * credentials = ::ui::copy_value(
		props.credentials ? *props.credentials : kEmptyLobbyConnectCredentials);
	if(!log || !credentials){
		return ::ui::empty();
	}
	::ui::UiElement frame = ::ui::provider(
		"LobbyConnectCredentialsProvider",
		&LobbyConnectCredentialsContext,
		const_cast<LobbyConnectCredentials *>(credentials),
		::ui::children({
			::ui::component("LobbyConnectFrame",
			                LobbyConnectFrameProps{ .key = "frame" },
			                LobbyConnectFrame),
		}),
		"credentials");
	return ::ui::provider(
		"LobbyConnectLogProvider",
		&LobbyConnectLogContext,
		const_cast<LobbyConnectLog *>(log),
		::ui::children({frame}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
