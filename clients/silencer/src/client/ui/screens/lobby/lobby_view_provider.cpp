#include "client/ui/screens/lobby/lobby_view.h"

#include "client/ui/screens/lobby/character_panel.h"
#include "client/ui/screens/lobby/chat_panel.h"
#include "client/ui/screens/lobby/game_create_panel.h"
#include "client/ui/screens/lobby/game_join_panel.h"
#include "client/ui/screens/lobby/game_select_panel.h"
#include "client/ui/screens/lobby/game_tech_panel.h"
#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {
namespace lobby {

namespace {
::ReactContext LobbyChromeContext = {};
::ReactContext LobbySurfaceContext = {};
::ReactContext LobbyNavigationContext = {};
const LobbyChrome kEmptyChrome = {};
const LobbySurface kEmptySurface = {};
const LobbyNavigation kEmptyNavigation = {};
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

::ui::UiElement LobbyScreenView(const LobbyScreenViewProps& props) {
	const LobbyChrome * chrome = ::ui::copy_value(
		props.chrome ? *props.chrome : kEmptyChrome);
	const LobbySurface * surface = ::ui::copy_value(
		props.surface ? *props.surface : kEmptySurface);
	const LobbyNavigation * navigation = ::ui::copy_value(
		props.navigation ? *props.navigation : kEmptyNavigation);
	if(!chrome || !surface || !navigation){
		return ::ui::empty();
	}
	::ui::UiElement frame = ::ui::component(
		"LobbyFrame",
		LobbyFrameProps{ .key = "view" },
		LobbyFrame);
	::ui::UiElement gameTechProvider = LobbyGameTechProvider(
		props.game_tech ? *props.game_tech : LobbyGameTech{},
		::ui::children({frame}),
		"game-tech");
	::ui::UiElement gameJoinProvider = LobbyGameJoinProvider(
		props.game_join ? *props.game_join : LobbyGameJoin{},
		::ui::children({gameTechProvider}),
		"game-join");
	::ui::UiElement gameCreateProvider = LobbyGameCreateProvider(
		props.game_create ? *props.game_create : LobbyGameCreate{},
		::ui::children({gameJoinProvider}),
		"game-create");
	::ui::UiElement gameSelectProvider = LobbyGameSelectProvider(
		props.game_select ? *props.game_select : LobbyGameSelect{},
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
