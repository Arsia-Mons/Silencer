#include "lobby_screen.h"

#include "game_create_panel.h"
#include "client/ui/ClientUi.h"
#include "hooks/use_lobby.h"
#include "lobby_chrome.h"
#include "lobby_main_area.h"

#include "screen_context.h"
#include "world.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "ui/game_ui_frame_provider.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>

namespace lobby_screen_detail {

constexpr int kLegacyLayoutW = 640;
constexpr int kLegacyLayoutH = 480;

int ClampInt(int value, int lo, int hi) {
	if(value < lo) return lo;
	if(value > hi) return hi;
	return value;
}

int ScaleLegacyPx(int base,
                  int actual,
                  int legacy,
                  int minValue,
                  int maxValue) {
	if(legacy <= 0) return ClampInt(base, minValue, maxValue);
	const int scaled = static_cast<int>((static_cast<long long>(base) * actual + legacy / 2) / legacy);
	return ClampInt(scaled, minValue, maxValue);
}

void AddPanelBorderBlur(ScreenContext & ctx, int x, int y, int w, int h) {
	if(w <= 0 || h <= 0) return;
	ctx.AddLobbyPanelBorderBlurRect(x, y, w, 1);
	ctx.AddLobbyPanelBorderBlurRect(x, y + h - 1, w, 1);
	ctx.AddLobbyPanelBorderBlurRect(x, y, 1, h);
	ctx.AddLobbyPanelBorderBlurRect(x + w - 1, y, 1, h);
}

std::function<void()> UseQueuedAction(std::function<void()> write)
{
	auto queueWrite = silencer::client_ui::UseUiWriteQueue();
	if(!queueWrite) return {};
	return [queueWrite, write]() {
		queueWrite(write);
	};
}

}  // namespace lobby_screen_detail

LobbyScreen::LobbyScreen() = default;
LobbyScreen::~LobbyScreen() = default;

void LobbyScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;
	ctx.ResetMenuPresentation(2);

	version  = "v.";
	version += world.GetVersion();
	mapName.clear();
	goBack = {};
	goBackQueued = false;

	silencer::client_ui::lobby::CharacterPanelInit(characterState, ctx);
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
	ctx.RequestLobbyGameListRefresh();
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
	(void)ctx;

	silencer::client_ui::lobby::GameTechPanelInit(gameTechState);
	gameTechActive   = true;
	gameJoinActive   = false;
	gameCreateActive = false;
}

void LobbyScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using namespace silencer::client_ui::lobby;

	const silencer::ui::UiInputState & input =
		silencer::game_ui::RequireGameUiFrame().input;
	const int layoutWidth = std::max(1, input.width);
	const int layoutHeight = std::max(1, input.height);
	const int rootPadX = lobby_screen_detail::ScaleLegacyPx(
		10, layoutWidth, lobby_screen_detail::kLegacyLayoutW, 8, 18);
	const int rootPadTop = lobby_screen_detail::ScaleLegacyPx(
		25, layoutHeight, lobby_screen_detail::kLegacyLayoutH, 18, 28);
	const int rootPadBottom = rootPadTop;
	const int regionGap = lobby_screen_detail::ScaleLegacyPx(
		10, layoutWidth, lobby_screen_detail::kLegacyLayoutW, 8, 16);
	const uint16_t titleBarH = LobbyTitleBarHeight();
	const int bodyW = std::max(0, layoutWidth - rootPadX * 2);
	const int bodyH = std::max(0, layoutHeight - rootPadTop - rootPadBottom
	                              - (int)titleBarH - regionGap);
	const int bodyX = rootPadX;
	const int bodyY = rootPadTop + (int)titleBarH + regionGap;
	goBackQueued = false;
	silencer::client_ui::hooks::LobbyProvider(ctx, [&]() {
		const silencer::client_ui::hooks::LobbyUi lobby =
			silencer::client_ui::hooks::UseLobby();
		goBack = lobby_screen_detail::UseQueuedAction([&ctx]() {
			ctx.GoBack();
		});
		gameJoinState.flushActions =
			[lobby, this, &ctx](
				std::shared_ptr<silencer::client_ui::hooks::LobbyGameJoinActions> actions) {
				if(!lobby.flushGameJoinActions || !actions) return;
				lobby.flushGameJoinActions(
					actions,
					[this]() { return gameJoinActive && !goBackQueued; },
					[this, &ctx]() { ShowGameTech(ctx); });
		};
		gameTechState.flushActions =
			[lobby, this, &ctx](
				std::shared_ptr<silencer::client_ui::hooks::LobbyGameTechActions> actions) {
				if(!lobby.flushGameTechActions || !actions) return;
				lobby.flushGameTechActions(
					actions,
					[this]() { return gameTechActive && !goBackQueued; },
					[this, &ctx]() { ShowGameJoin(ctx); });
		};
		gameSelectState.flushActions =
			[lobby, this, &ctx](
				std::shared_ptr<silencer::client_ui::hooks::LobbyGameSelectActions> actions) {
				if(!lobby.flushGameSelectActions || !actions) return;
				lobby.flushGameSelectActions(
					actions,
					[this]() {
						return !gameCreateActive
						    && !gameJoinActive
						    && !gameTechActive
						    && !goBackQueued;
					},
					[this]() -> uint32_t {
						if(gameSelectState.selectedIndex < 0
						   || gameSelectState.selectedIndex >=
						      static_cast<int>(gameSelectState.rows.size())){
							return 0;
						}
						return gameSelectState.rows[gameSelectState.selectedIndex].gameid;
					},
					[this, &ctx]() { ShowGameCreate(ctx); });
		};

		ctx.BeginLobbyPanelBorderBlur(layoutWidth, layoutHeight, input.uiScale);
		lobby_screen_detail::AddPanelBorderBlur(
			ctx,
			rootPadX,
			rootPadTop,
			bodyW,
			static_cast<int>(titleBarH));

		CLAY({ .id = CLAY_ID("LobbyRoot"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_GROW(0) },
		           .padding = { static_cast<uint16_t>(rootPadX),
		                        static_cast<uint16_t>(rootPadX),
		                        static_cast<uint16_t>(rootPadTop),
		                        static_cast<uint16_t>(rootPadBottom) },
		           .childGap = static_cast<uint16_t>(regionGap),
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = silencer::clay_bridge::PackImageStretch(7, 1) } }) {
			BuildLobbyTitleBar(version, mapName, layoutWidth, interactions);

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
			BuildLobbyMainArea(
				panels,
				ctx,
				ctx.world.resources,
				bodyX,
				bodyY,
				bodyW,
				bodyH,
				regionGap,
				interactions);
			if(gameCreateActive){
				BuildGameCreatePreviewOverlay(gameCreateState, ctx);
			}
		}
	});
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
	goBack = {};
	goBackQueued = false;
}

void LobbyScreen::SetMapNameOverlay(const char * name)
{
	mapName = name ? std::string(name).substr(0, 25) : std::string();
}
