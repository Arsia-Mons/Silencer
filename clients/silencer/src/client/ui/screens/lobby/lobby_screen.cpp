#include "lobby_screen.h"

#include "game_create_panel.h"
#include "lobby_chrome.h"
#include "lobby_main_area.h"

#include "screen_context.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace lobby_screen_detail {

constexpr uint16_t kRootPadX = 10;
constexpr uint16_t kRootPadTop = 25;
constexpr uint16_t kRootPadBottom = 25;
constexpr uint16_t kRegionGap = 10;

}  // namespace lobby_screen_detail

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

void LobbyScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	using namespace silencer::clay_bridge;
	using namespace silencer::client_ui::lobby;

	const bool narrow = LobbyUseNarrowLayout(dst.w);
	const uint16_t titleBarH = LobbyTitleBarHeight(narrow, mapName);
	const int bodyH = std::max(0, dst.h - (int)lobby_screen_detail::kRootPadTop - (int)lobby_screen_detail::kRootPadBottom
	                              - (int)titleBarH - (int)lobby_screen_detail::kRegionGap);

	CLAY({ .id = CLAY_ID("LobbyRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .padding = { lobby_screen_detail::kRootPadX, lobby_screen_detail::kRootPadX, lobby_screen_detail::kRootPadTop, lobby_screen_detail::kRootPadBottom },
	           .childGap = lobby_screen_detail::kRegionGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	       .image = { .imageData = PackImageStretch(7, 1) } }) {
		BuildLobbyTitleBar(version, mapName, narrow, dst.w, interactions);

		LobbyMainAreaPanels panels{
			characterState,
			chatState,
			gameSelectState,
			gameCreateState,
			gameJoinState,
			gameTechState,
			gameCreateActive,
			gameJoinActive,
			gameTechActive,
		};
		BuildLobbyMainArea(panels, ctx, *this, narrow, bodyH, interactions);
		if(gameCreateActive){
			BuildGameCreatePreviewOverlay(gameCreateState, ctx);
		}
	}
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

void LobbyScreen::SetMapNameOverlay(World & /*world*/, const char * name)
{
	mapName = name ? std::string(name).substr(0, 25) : std::string();
}
