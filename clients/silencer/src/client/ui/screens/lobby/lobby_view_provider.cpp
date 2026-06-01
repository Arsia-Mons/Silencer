#include "client/ui/screens/lobby/lobby_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {
namespace lobby {

namespace {
::ReactContext LobbyContext = {};
::ReactContext LobbyNavigationContext = {};
const LobbyContextValue kEmptyLobby = {};
const LobbyNavigation kEmptyNavigation = {};
}  // namespace

const LobbyContextValue& UseLobby() {
	const auto * value = static_cast<const LobbyContextValue *>(
		::use_context(&LobbyContext));
	return value ? *value : kEmptyLobby;
}

const LobbyNavigation& UseLobbyNavigation() {
	const auto * value = static_cast<const LobbyNavigation *>(
		::use_context(&LobbyNavigationContext));
	return value ? *value : kEmptyNavigation;
}

::ui::UiElement LobbyScreenView(const LobbyScreenViewProps& props) {
	const LobbyContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyLobby);
	const LobbyNavigation * navigation = ::ui::copy_value(
		props.navigation ? *props.navigation : kEmptyNavigation);
	if(!stored || !navigation){
		return ::ui::empty();
	}
	::ui::UiElement frame = ::ui::component(
		"LobbyFrame",
		LobbyFrameProps{ .key = "view" },
		LobbyFrame);
	::ui::UiElement navigationProvider = ::ui::provider(
		"LobbyNavigationProvider",
		&LobbyNavigationContext,
		const_cast<LobbyNavigation *>(navigation),
		::ui::children({frame}),
		"navigation");
	return ::ui::provider(
		"LobbyProvider",
		&LobbyContext,
		const_cast<LobbyContextValue *>(stored),
		::ui::children({navigationProvider}),
		props.key);
}

}  // namespace lobby
}  // namespace client_ui
}  // namespace silencer
