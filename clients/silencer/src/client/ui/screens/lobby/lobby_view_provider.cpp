#include "client/ui/screens/lobby/lobby_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {
namespace lobby {

namespace {
::ReactContext LobbyContext = {};
::ReactContext LobbyNavigationContext = {};
::ReactContext LobbyChatContext = {};
::ReactContext LobbyCharacterContext = {};
const LobbyContextValue kEmptyLobby = {};
const LobbyNavigation kEmptyNavigation = {};
const LobbyChat kEmptyChat = {};
const LobbyCharacter kEmptyCharacter = {};
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

const LobbyChat& UseLobbyChat() {
	const auto * value = static_cast<const LobbyChat *>(
		::use_context(&LobbyChatContext));
	return value ? *value : kEmptyChat;
}

const LobbyCharacter& UseLobbyCharacter() {
	const auto * value = static_cast<const LobbyCharacter *>(
		::use_context(&LobbyCharacterContext));
	return value ? *value : kEmptyCharacter;
}

::ui::UiElement LobbyScreenView(const LobbyScreenViewProps& props) {
	const LobbyContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyLobby);
	const LobbyNavigation * navigation = ::ui::copy_value(
		props.navigation ? *props.navigation : kEmptyNavigation);
	const LobbyChat * chat = ::ui::copy_value(
		props.chat ? *props.chat : kEmptyChat);
	const LobbyCharacter * character = ::ui::copy_value(
		props.character ? *props.character : kEmptyCharacter);
	if(!stored || !navigation || !chat || !character){
		return ::ui::empty();
	}
	::ui::UiElement frame = ::ui::component(
		"LobbyFrame",
		LobbyFrameProps{ .key = "view" },
		LobbyFrame);
	::ui::UiElement characterProvider = ::ui::provider(
		"LobbyCharacterProvider",
		&LobbyCharacterContext,
		const_cast<LobbyCharacter *>(character),
		::ui::children({frame}),
		"character");
	::ui::UiElement chatProvider = ::ui::provider(
		"LobbyChatProvider",
		&LobbyChatContext,
		const_cast<LobbyChat *>(chat),
		::ui::children({characterProvider}),
		"chat");
	::ui::UiElement navigationProvider = ::ui::provider(
		"LobbyNavigationProvider",
		&LobbyNavigationContext,
		const_cast<LobbyNavigation *>(navigation),
		::ui::children({chatProvider}),
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
