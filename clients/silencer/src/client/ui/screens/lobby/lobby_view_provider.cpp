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
::ReactContext LobbyGameSelectContext = {};
::ReactContext LobbyGameCreateContext = {};
::ReactContext LobbyGameJoinContext = {};
::ReactContext LobbyGameTechContext = {};
const LobbyContextValue kEmptyLobby = {};
const LobbyNavigation kEmptyNavigation = {};
const LobbyChat kEmptyChat = {};
const LobbyCharacter kEmptyCharacter = {};
const LobbyGameSelect kEmptyGameSelect = {};
const LobbyGameCreate kEmptyGameCreate = {};
const LobbyGameJoin kEmptyGameJoin = {};
const LobbyGameTech kEmptyGameTech = {};
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

const LobbyGameSelect& UseLobbyGameSelect() {
	const auto * value = static_cast<const LobbyGameSelect *>(
		::use_context(&LobbyGameSelectContext));
	return value ? *value : kEmptyGameSelect;
}

const LobbyGameCreate& UseLobbyGameCreate() {
	const auto * value = static_cast<const LobbyGameCreate *>(
		::use_context(&LobbyGameCreateContext));
	return value ? *value : kEmptyGameCreate;
}

const LobbyGameJoin& UseLobbyGameJoin() {
	const auto * value = static_cast<const LobbyGameJoin *>(
		::use_context(&LobbyGameJoinContext));
	return value ? *value : kEmptyGameJoin;
}

const LobbyGameTech& UseLobbyGameTech() {
	const auto * value = static_cast<const LobbyGameTech *>(
		::use_context(&LobbyGameTechContext));
	return value ? *value : kEmptyGameTech;
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
	const LobbyGameSelect * gameSelect = ::ui::copy_value(
		props.game_select ? *props.game_select : kEmptyGameSelect);
	const LobbyGameCreate * gameCreate = ::ui::copy_value(
		props.game_create ? *props.game_create : kEmptyGameCreate);
	const LobbyGameJoin * gameJoin = ::ui::copy_value(
		props.game_join ? *props.game_join : kEmptyGameJoin);
	const LobbyGameTech * gameTech = ::ui::copy_value(
		props.game_tech ? *props.game_tech : kEmptyGameTech);
	if(!stored || !navigation || !chat || !character || !gameSelect || !gameCreate || !gameJoin || !gameTech){
		return ::ui::empty();
	}
	::ui::UiElement frame = ::ui::component(
		"LobbyFrame",
		LobbyFrameProps{ .key = "view" },
		LobbyFrame);
	::ui::UiElement gameTechProvider = ::ui::provider(
		"LobbyGameTechProvider",
		&LobbyGameTechContext,
		const_cast<LobbyGameTech *>(gameTech),
		::ui::children({frame}),
		"game-tech");
	::ui::UiElement gameJoinProvider = ::ui::provider(
		"LobbyGameJoinProvider",
		&LobbyGameJoinContext,
		const_cast<LobbyGameJoin *>(gameJoin),
		::ui::children({gameTechProvider}),
		"game-join");
	::ui::UiElement gameCreateProvider = ::ui::provider(
		"LobbyGameCreateProvider",
		&LobbyGameCreateContext,
		const_cast<LobbyGameCreate *>(gameCreate),
		::ui::children({gameJoinProvider}),
		"game-create");
	::ui::UiElement gameSelectProvider = ::ui::provider(
		"LobbyGameSelectProvider",
		&LobbyGameSelectContext,
		const_cast<LobbyGameSelect *>(gameSelect),
		::ui::children({gameCreateProvider}),
		"game-select");
	::ui::UiElement characterProvider = ::ui::provider(
		"LobbyCharacterProvider",
		&LobbyCharacterContext,
		const_cast<LobbyCharacter *>(character),
		::ui::children({gameSelectProvider}),
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
