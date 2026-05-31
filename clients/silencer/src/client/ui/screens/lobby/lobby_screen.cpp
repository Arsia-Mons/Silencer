#include "lobby_screen.h"

#include "client/ui/screens/lobby/lobby_view.h"

#include "screen_context.h"
#include "game.h"
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
	silencer::client_ui::lobby::LobbyContextValue context{
		.version = version.c_str(),
		.map_name = mapName.c_str(),
		.world = &ctx.world,
		.character = &characterState,
		.chat = &chatState,
		.game_select = &gameSelectState,
		.game_create = &gameCreateState,
		.game_join = &gameJoinState,
		.game_tech = &gameTechState,
		.game_create_active = gameCreateActive,
		.game_join_active = gameJoinActive,
		.game_tech_active = gameTechActive,
		.go_back_clicked = &goBackClicked,
		.chat_send_clicked = &chatSendClicked,
	};
	*out = silencer::client_ui::lobby::LobbyScreenView(
		silencer::client_ui::lobby::LobbyScreenViewProps{
			.key = "lobby",
			.value = ::ui::copy_value(context),
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
