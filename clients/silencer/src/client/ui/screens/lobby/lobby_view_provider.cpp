#include "client/ui/screens/lobby/lobby_view.h"

#include "client/ui/screens/lobby/character_panel.h"
#include "client/ui/screens/lobby/chat_panel.h"
#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {
namespace lobby {

namespace {
::ReactContext LobbyChromeContext = {};
::ReactContext LobbySurfaceContext = {};
::ReactContext LobbyNavigationContext = {};
::ReactContext LobbyGameSelectContext = {};
::ReactContext LobbyGameCreateContext = {};
::ReactContext LobbyGameJoinContext = {};
::ReactContext LobbyGameTechContext = {};
const LobbyChrome kEmptyChrome = {};
const LobbySurface kEmptySurface = {};
const LobbyNavigation kEmptyNavigation = {};
const LobbyGameSelect kEmptyGameSelect = {};
const LobbyGameCreate kEmptyGameCreate = {};
const LobbyGameJoin kEmptyGameJoin = {};
const LobbyGameTech kEmptyGameTech = {};
}  // namespace

const LobbyChrome& UseLobbyChrome() {
	const auto * value = static_cast<const LobbyChrome *>(
		::use_context(&LobbyChromeContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyChromeProvider for UseLobbyChrome\n");
	return kEmptyChrome;
}

const LobbySurface& UseLobbySurface() {
	const auto * value = static_cast<const LobbySurface *>(
		::use_context(&LobbySurfaceContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbySurfaceProvider for UseLobbySurface\n");
	return kEmptySurface;
}

const LobbyNavigation& UseLobbyNavigation() {
	const auto * value = static_cast<const LobbyNavigation *>(
		::use_context(&LobbyNavigationContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyNavigationProvider for UseLobbyNavigation\n");
	return kEmptyNavigation;
}

const LobbyGameSelect& UseLobbyGameSelect() {
	const auto * value = static_cast<const LobbyGameSelect *>(
		::use_context(&LobbyGameSelectContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyGameSelectProvider for UseLobbyGameSelect\n");
	return kEmptyGameSelect;
}

const LobbyGameCreate& UseLobbyGameCreate() {
	const auto * value = static_cast<const LobbyGameCreate *>(
		::use_context(&LobbyGameCreateContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyGameCreateProvider for UseLobbyGameCreate\n");
	return kEmptyGameCreate;
}

const LobbyGameJoin& UseLobbyGameJoin() {
	const auto * value = static_cast<const LobbyGameJoin *>(
		::use_context(&LobbyGameJoinContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyGameJoinProvider for UseLobbyGameJoin\n");
	return kEmptyGameJoin;
}

const LobbyGameTech& UseLobbyGameTech() {
	const auto * value = static_cast<const LobbyGameTech *>(
		::use_context(&LobbyGameTechContext));
	if(value) return *value;
	::react_report_error("client/ui/lobby: missing LobbyGameTechProvider for UseLobbyGameTech\n");
	return kEmptyGameTech;
}

::ui::UiElement LobbyScreenView(const LobbyScreenViewProps& props) {
	const LobbyChrome * chrome = ::ui::copy_value(
		props.chrome ? *props.chrome : kEmptyChrome);
	const LobbySurface * surface = ::ui::copy_value(
		props.surface ? *props.surface : kEmptySurface);
	const LobbyNavigation * navigation = ::ui::copy_value(
		props.navigation ? *props.navigation : kEmptyNavigation);
	const LobbyGameSelect * gameSelect = ::ui::copy_value(
		props.game_select ? *props.game_select : kEmptyGameSelect);
	const LobbyGameCreate * gameCreate = ::ui::copy_value(
		props.game_create ? *props.game_create : kEmptyGameCreate);
	const LobbyGameJoin * gameJoin = ::ui::copy_value(
		props.game_join ? *props.game_join : kEmptyGameJoin);
	const LobbyGameTech * gameTech = ::ui::copy_value(
		props.game_tech ? *props.game_tech : kEmptyGameTech);
	if(!chrome || !surface || !navigation || !gameSelect || !gameCreate || !gameJoin || !gameTech){
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
	::ui::UiElement characterProvider = LobbyCharacterProvider(
		props.character ? *props.character : LobbyCharacter{},
		::ui::children({gameSelectProvider}),
		"character");
	::ui::UiElement chatProvider = LobbyChatProvider(
		props.chat ? *props.chat : LobbyChat{},
		::ui::children({characterProvider}),
		"chat");
	::ui::UiElement navigationProvider = ::ui::provider(
		"LobbyNavigationProvider",
		&LobbyNavigationContext,
		const_cast<LobbyNavigation *>(navigation),
		::ui::children({chatProvider}),
		"navigation");
	::ui::UiElement surfaceProvider = ::ui::provider(
		"LobbySurfaceProvider",
		&LobbySurfaceContext,
		const_cast<LobbySurface *>(surface),
		::ui::children({navigationProvider}),
		"surface");
	return ::ui::provider(
		"LobbyChromeProvider",
		&LobbyChromeContext,
		const_cast<LobbyChrome *>(chrome),
		::ui::children({surfaceProvider}),
		props.key);
}

}  // namespace lobby
}  // namespace client_ui
}  // namespace silencer
