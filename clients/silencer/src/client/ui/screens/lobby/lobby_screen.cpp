#include "lobby_screen.h"

#include "client/ui/screens/lobby/lobby_view.h"

#include "screen_context.h"
#include "game.h"
#include "lobby.h"
#include "world.h"
#include "renderer.h"

#include <algorithm>
#include <cstring>
#include <string>

LobbyScreen::LobbyScreen() = default;
LobbyScreen::~LobbyScreen() = default;

void LobbyScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;
	world.lobby.ForgetAllUserInfo();
	world.SetLobbyGameplayState();
	ctx.UnloadGame();
	world.Disconnect();
	world.choosingtech = false;
	world.lobby.channelchanged = true;
	ctx.ResetPresentation(2);
	ctx.renderer.camera.SetPosition(320, 240);

	version  = "v.";
	version += world.GetVersion();
	mapName.clear();
	goBackClicked = false;
	chatSendClicked = false;

	silencer::client_ui::lobby::CharacterPanelInit(characterState);
	silencer::client_ui::lobby::ChatPanelInit(chatState);
	silencer::client_ui::lobby::GameSelectPanelInit(gameSelectState);
	silencer::client_ui::lobby::GameJoinPanelInit(gameJoinState);
	gameJoinActive = false;
	silencer::client_ui::lobby::GameTechPanelInit(gameTechState);
	gameTechActive = false;
	gameCreateActive = false;
}

void LobbyScreen::ShowGameSelect(ScreenContext & ctx)
{
	gameCreateActive = false;
	gameJoinActive   = false;
	gameTechActive   = false;
	ctx.world.lobby.gamesprocessed = false;
}

void LobbyScreen::ShowGameCreate(ScreenContext & ctx)
{
	silencer::client_ui::lobby::GameCreatePanelInit(gameCreateState, ctx);
	gameCreateActive = true;
	gameJoinActive   = false;
	gameTechActive   = false;
}

void LobbyScreen::ShowGameJoin(ScreenContext & ctx)
{
	silencer::client_ui::lobby::GameJoinPanelInit(gameJoinState);
	gameJoinActive   = true;
	gameCreateActive = false;
	gameTechActive   = false;
}

void LobbyScreen::ShowGameTech(ScreenContext & ctx)
{
	ctx.world.choosingtech = true;
	ctx.world.RequestPeerList();

	silencer::client_ui::lobby::GameTechPanelInit(gameTechState);
	gameTechActive   = true;
	gameJoinActive   = false;
	gameCreateActive = false;
}

bool LobbyScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;
	const silencer::ui::UiInputState & input = ctx.game.CurrentUiInput();
	const int layoutWidth = std::max(1, input.width);
	const int layoutHeight = std::max(1, input.height);
	const int bodyWidth = std::max(1, layoutWidth - 20);
	const int bodyHeight = std::max(1, layoutHeight - 80);
	const int chatWidth = std::max(220, bodyWidth - 244 - 10);
	const int chatHeight = std::max(120, bodyHeight - 126 - 10);
	silencer::client_ui::lobby::ChatPanelSyncLayout(
		chatState,
		static_cast<Uint16>(std::min(chatWidth, 1024)),
		static_cast<Uint16>(std::min(chatHeight, 768)));
	silencer::client_ui::lobby::LobbyChrome chrome{
		.version = version.c_str(),
		.map_name = mapName.c_str(),
	};
	silencer::client_ui::lobby::LobbySurface surface{
		.game_create_active = gameCreateActive,
		.game_join_active = gameJoinActive,
		.game_tech_active = gameTechActive,
	};
	silencer::client_ui::lobby::LobbyNavigation navigation{
		.go_back = [this]() {
			goBackClicked = true;
		},
	};
	silencer::client_ui::lobby::LobbyChat chat{
		.state = &chatState,
		.set_text = [this](const std::string& value) {
			silencer::client_ui::lobby::ChatPanelSetInput(chatState, value);
		},
		.send = [this]() {
			chatSendClicked = true;
		},
	};
	silencer::client_ui::lobby::LobbyCharacter character{
		.state = &characterState,
		.change_agent = [this]() {
			if(!characterState.agentSelectionLocked){
				characterState.newCharacterRequested = true;
			}
		},
	};
	silencer::client_ui::lobby::LobbyGameSelect gameSelect{
		.state = &gameSelectState,
		.select = [this](int index) {
			silencer::client_ui::lobby::GameSelectPanelSelectRow(gameSelectState, index);
		},
		.scroll = [this](int delta) {
			silencer::client_ui::lobby::GameSelectPanelScrollRows(gameSelectState, delta);
		},
		.create = [this]() {
			silencer::client_ui::lobby::GameSelectPanelRequestCreate(gameSelectState);
		},
		.join = [this]() {
			silencer::client_ui::lobby::GameSelectPanelRequestJoin(gameSelectState);
		},
		.spectate = [this]() {
			silencer::client_ui::lobby::GameSelectPanelRequestSpectate(gameSelectState);
		},
	};
	silencer::client_ui::lobby::LobbyGameCreate gameCreate{
		.state = &gameCreateState,
		.select_map = [this](int index) {
			silencer::client_ui::lobby::GameCreatePanelSelectMap(gameCreateState, index);
		},
		.scroll_maps = [this](int delta) {
			silencer::client_ui::lobby::GameCreatePanelScrollMaps(gameCreateState, delta);
		},
		.cycle_security = [this]() {
			silencer::client_ui::lobby::GameCreatePanelCycleSecurity(gameCreateState);
		},
		.toggle_spectatable = [this]() {
			silencer::client_ui::lobby::GameCreatePanelToggleSpectatable(gameCreateState);
		},
		.submit = [this]() {
			silencer::client_ui::lobby::GameCreatePanelRequestCreate(gameCreateState);
		},
		.set_name = [this](const std::string& value) {
			silencer::client_ui::lobby::GameCreatePanelSetName(gameCreateState, value);
		},
		.set_password = [this](const std::string& value) {
			silencer::client_ui::lobby::GameCreatePanelSetPassword(gameCreateState, value);
		},
		.set_min_level = [this](const std::string& value) {
			silencer::client_ui::lobby::GameCreatePanelSetMinLevel(gameCreateState, value);
		},
		.set_max_level = [this](const std::string& value) {
			silencer::client_ui::lobby::GameCreatePanelSetMaxLevel(gameCreateState, value);
		},
		.set_max_players = [this](const std::string& value) {
			silencer::client_ui::lobby::GameCreatePanelSetMaxPlayers(gameCreateState, value);
		},
		.set_max_teams = [this](const std::string& value) {
			silencer::client_ui::lobby::GameCreatePanelSetMaxTeams(gameCreateState, value);
		},
	};
	silencer::client_ui::lobby::LobbyGameJoin gameJoin{
		.state = &gameJoinState,
		.choose_tech = [this]() {
			silencer::client_ui::lobby::GameJoinPanelRequestTech(gameJoinState);
		},
		.change_team = [this]() {
			silencer::client_ui::lobby::GameJoinPanelRequestTeam(gameJoinState);
		},
		.ready = [this]() {
			silencer::client_ui::lobby::GameJoinPanelRequestReady(gameJoinState);
		},
	};
	silencer::client_ui::lobby::LobbyGameTech gameTech{
		.state = &gameTechState,
		.back_to_team = [this]() {
			silencer::client_ui::lobby::GameTechPanelRequestBack(gameTechState);
		},
		.preview = [this](int index) {
			silencer::client_ui::lobby::GameTechPanelPreviewItem(gameTechState, index);
		},
		.toggle = [this](int index) {
			silencer::client_ui::lobby::GameTechPanelToggleItem(gameTechState, index);
		},
	};
	*out = silencer::client_ui::lobby::LobbyScreenView(
		silencer::client_ui::lobby::LobbyScreenViewProps{
			.key = "lobby",
			.chrome = ::ui::copy_value(chrome),
			.surface = ::ui::copy_value(surface),
			.navigation = ::ui::copy_value(navigation),
			.chat = ::ui::copy_value(chat),
			.character = ::ui::copy_value(character),
			.game_select = ::ui::copy_value(gameSelect),
			.game_create = ::ui::copy_value(gameCreate),
			.game_join = ::ui::copy_value(gameJoin),
			.game_tech = ::ui::copy_value(gameTech),
		});
	return true;
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

void LobbyScreen::SetMapNameOverlay(World & /*world*/, const char * name)
{
	mapName = name ? std::string(name).substr(0, 25) : std::string();
}
