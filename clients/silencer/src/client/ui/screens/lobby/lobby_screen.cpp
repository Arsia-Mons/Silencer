#include "lobby_screen.h"

#include "character_create_screen.h"
#include "game_create_panel.h"
#include "client/ui/hooks/use_app.h"
#include "client/ui/hooks/use_lobby.h"
#include "client/ui/hooks/use_navigation.h"
#include "client/ui/screens/lobby/lobby_chrome_frame.h"
#include "lobby_connect_screen.h"
#include "lobby_main_area.h"
#include "main_menu_screen.h"

#include "screen_context.h"
#include "game.h"
#include "message_modal.h"
#include "renderdevice.h"
#include "renderer.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

namespace lobby_screen_detail {

constexpr int kLegacyLayoutW = 640;
constexpr int kLegacyLayoutH = 480;
constexpr int kLobbyTitleBarH = 29;
constexpr int kGameSelectCreatePadLeft = 4;
constexpr int kGameSelectCreatePadRight = 4;
constexpr int kGameSelectCreatePadTop = 4;
constexpr int kGameSelectCreateButtonH = 21;
constexpr int kGameSelectInfoRows = 5;
constexpr int kGameSelectInfoRowH = 12;
constexpr int kGameSelectTallFooterPadTop = 4;
constexpr int kGameSelectTallFooterGap = 4;
constexpr int kGameSelectActionButtonGap = 2;
constexpr int kGameSelectActionButtonW = 156;
constexpr int kGameSelectActionButtonH = 21;
constexpr int kGameJoinButtonPadLeft = 3;
constexpr int kGameJoinButtonPadRight = 4;
constexpr int kGameJoinChooseTechPadTop = 3;
constexpr int kGameJoinChangeTeamPadTop = 11;
constexpr int kGameJoinReadyPadTop = 39;
constexpr int kGameJoinButtonH = 21;
constexpr int kGameTechBackPadLeft = 4;
constexpr int kGameTechBackPadRight = 4;
constexpr int kGameTechBackPadTop = 4;
constexpr int kGameTechButtonH = 21;
constexpr int kGameTechPeerColPadLeft = 4;
constexpr int kGameTechPeerColPadRight = 4;
constexpr int kGameTechPeerColPadTop = 7;
constexpr int kGameTechPeerRowGap = 5;

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

void AddPanelBorderBlur(RenderDevice * renderdevice, SDL_Rect rect) {
	if(!renderdevice || rect.w <= 0 || rect.h <= 0) return;
	renderdevice->AddLobbyPanelBorderBlurRect({ rect.x, rect.y, rect.w, 1 });
	renderdevice->AddLobbyPanelBorderBlurRect({ rect.x, rect.y + rect.h - 1, rect.w, 1 });
	renderdevice->AddLobbyPanelBorderBlurRect({ rect.x, rect.y, 1, rect.h });
	renderdevice->AddLobbyPanelBorderBlurRect({ rect.x + rect.w - 1, rect.y, 1, rect.h });
}

}  // namespace lobby_screen_detail

LobbyScreen::LobbyScreen() = default;
LobbyScreen::~LobbyScreen() = default;

void LobbyScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(2);
	ctx.renderer.camera.SetPosition(320, 240);

	version  = "v.";
	version += silencer::client_ui::use_app(
		silencer::client_ui::MakeAppProvider(ctx)).version();
	mapName.clear();
	goBackClicked = false;
	disconnectMessageOpen = false;

	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx, this));
	silencer::client_ui::lobby::CharacterPanelInit(characterState, lobby.character);
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
	(void)ctx;
	gameCreateActive = false;
	gameJoinActive   = false;
	gameTechActive   = false;
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

	const silencer::ui::UiInputState & input = ctx.game.CurrentUiInput();
	const int layoutWidth = std::max(1, input.width);
	const int layoutHeight = std::max(1, input.height);
	const int rootPadX = lobby_screen_detail::ScaleLegacyPx(
		10, layoutWidth, lobby_screen_detail::kLegacyLayoutW, 8, 18);
	const int rootPadTop = lobby_screen_detail::ScaleLegacyPx(
		25, layoutHeight, lobby_screen_detail::kLegacyLayoutH, 18, 28);
	const int rootPadBottom = rootPadTop;
	const int regionGap = lobby_screen_detail::ScaleLegacyPx(
		10, layoutWidth, lobby_screen_detail::kLegacyLayoutW, 8, 16);
	const uint16_t titleBarH = lobby_screen_detail::kLobbyTitleBarH;
	const int bodyW = std::max(0, layoutWidth - rootPadX * 2);
	const int bodyH = std::max(0, layoutHeight - rootPadTop - rootPadBottom
	                              - (int)titleBarH - regionGap);
	const int bodyX = rootPadX;
	const int bodyY = rootPadTop + (int)titleBarH + regionGap;
	const LobbyMainAreaLayout mainLayout =
		ResolveLobbyMainAreaLayout(bodyW, bodyH, regionGap);
	const bool gameSelectVisible =
		!gameCreateActive && !gameJoinActive && !gameTechActive;
	const int createButtonW = std::max(
		1,
		mainLayout.rightUpperW
			- lobby_screen_detail::kGameSelectCreatePadLeft
			- lobby_screen_detail::kGameSelectCreatePadRight);
	const bool showGameSelectCreate =
		gameSelectVisible &&
		mainLayout.rightUpperW > lobby_screen_detail::kGameSelectCreatePadLeft
			+ lobby_screen_detail::kGameSelectCreatePadRight &&
		mainLayout.upperH > lobby_screen_detail::kGameSelectCreatePadTop
			+ lobby_screen_detail::kGameSelectCreateButtonH;
	const int gameSelectInfoH =
		lobby_screen_detail::kGameSelectInfoRows
			* lobby_screen_detail::kGameSelectInfoRowH;
	const int gameSelectActionsH =
		lobby_screen_detail::kGameSelectActionButtonH * 2
			+ lobby_screen_detail::kGameSelectActionButtonGap;
	const int gameSelectFooterH =
		lobby_screen_detail::kGameSelectTallFooterPadTop
			+ gameSelectInfoH
			+ lobby_screen_detail::kGameSelectTallFooterGap
			+ gameSelectActionsH;
	const bool showGameSelectActions =
		gameSelectVisible &&
		mainLayout.rightTallW >= lobby_screen_detail::kGameSelectActionButtonW &&
		mainLayout.rightTallH >= gameSelectFooterH;
	const int actionButtonX =
		bodyX + mainLayout.topRowW
			+ std::max(
				0,
				(mainLayout.rightTallW
					- lobby_screen_detail::kGameSelectActionButtonW) / 2);
	const int spectateButtonY =
		bodyY + mainLayout.rightTallH - gameSelectActionsH;
	const int joinButtonY =
		spectateButtonY
			+ lobby_screen_detail::kGameSelectActionButtonH
			+ lobby_screen_detail::kGameSelectActionButtonGap;
	const int rightUpperX = bodyX + mainLayout.characterW + mainLayout.regionGap;
	const int gameJoinButtonW = std::max(
		1,
		mainLayout.rightUpperW
			- lobby_screen_detail::kGameJoinButtonPadLeft
			- lobby_screen_detail::kGameJoinButtonPadRight);
	const int gameJoinChooseTechY =
		bodyY + lobby_screen_detail::kGameJoinChooseTechPadTop;
	const int gameJoinChangeTeamY =
		bodyY
			+ lobby_screen_detail::kGameJoinChooseTechPadTop
			+ lobby_screen_detail::kGameJoinButtonH
			+ lobby_screen_detail::kGameJoinChangeTeamPadTop;
	const int gameJoinReadyY =
		gameJoinChangeTeamY
			+ lobby_screen_detail::kGameJoinButtonH
			+ lobby_screen_detail::kGameJoinReadyPadTop;
	const bool showGameJoinActions =
		gameJoinActive &&
		mainLayout.rightUpperW > lobby_screen_detail::kGameJoinButtonPadLeft
			+ lobby_screen_detail::kGameJoinButtonPadRight &&
		mainLayout.upperH > gameJoinReadyY - bodyY
			+ lobby_screen_detail::kGameJoinButtonH;
	const int gameTechBackW = std::max(
		1,
		mainLayout.rightUpperW
			- lobby_screen_detail::kGameTechBackPadLeft
			- lobby_screen_detail::kGameTechBackPadRight);
	const int gameTechPeerW = std::max(
		1,
		mainLayout.rightUpperW
			- lobby_screen_detail::kGameTechPeerColPadLeft
			- lobby_screen_detail::kGameTechPeerColPadRight);
	const int gameTechBackY = bodyY + lobby_screen_detail::kGameTechBackPadTop;
	const int gameTechPeerY =
		bodyY
			+ lobby_screen_detail::kGameTechBackPadTop
			+ lobby_screen_detail::kGameTechButtonH
			+ lobby_screen_detail::kGameTechPeerColPadTop;
	const bool showGameTechUpper =
		gameTechActive &&
		mainLayout.rightUpperW > lobby_screen_detail::kGameTechBackPadLeft
			+ lobby_screen_detail::kGameTechBackPadRight &&
		mainLayout.upperH > gameTechPeerY - bodyY + 11;
	const uint16_t titlePadX = static_cast<uint16_t>(
		lobby_screen_detail::ClampInt((layoutWidth * 5) / 640, 5, 10));
	const uint16_t titleRowGap = static_cast<uint16_t>(
		lobby_screen_detail::ClampInt((layoutWidth * 6) / 640, 4, 10));
	const bool showMapName = !mapName.empty() && layoutWidth >= 700;

	silencer::client_ui::lobby::LobbyChromeFrameProps chromeProps{
		.key = "lobby-chrome",
		.version = version.c_str(),
		.map_name = mapName.c_str(),
		.show_map_name = showMapName,
		.x = rootPadX,
		.y = rootPadTop,
		.width = bodyW,
		.height = titleBarH,
		.pad_x = titlePadX,
		.row_gap = titleRowGap,
		.show_game_select_create = showGameSelectCreate,
		.game_select_create_x = bodyX + mainLayout.characterW + mainLayout.regionGap
		                        + lobby_screen_detail::kGameSelectCreatePadLeft,
		.game_select_create_y = bodyY + lobby_screen_detail::kGameSelectCreatePadTop,
		.game_select_create_width = createButtonW,
		.game_select_create_height = lobby_screen_detail::kGameSelectCreateButtonH,
		.show_game_select_spectate =
			showGameSelectActions && gameSelectState.spectateVisible,
		.show_game_select_join =
			showGameSelectActions && gameSelectState.joinVisible,
		.game_select_spectate_x = actionButtonX,
		.game_select_spectate_y = spectateButtonY,
		.game_select_join_x = actionButtonX,
		.game_select_join_y = joinButtonY,
		.game_select_action_width = lobby_screen_detail::kGameSelectActionButtonW,
		.game_select_action_height = lobby_screen_detail::kGameSelectActionButtonH,
		.show_game_select_tall = gameSelectVisible,
		.game_select_tall_x = bodyX + mainLayout.topRowW,
		.game_select_tall_y = bodyY,
		.game_select_tall_width = mainLayout.rightTallW,
		.game_select_tall_height = mainLayout.rightTallH,
		.game_select = &gameSelectState,
		.show_game_join_actions = showGameJoinActions,
		.game_join_ready_label = gameJoinState.readyLabel.c_str(),
		.game_join_button_x = rightUpperX + lobby_screen_detail::kGameJoinButtonPadLeft,
		.game_join_choose_tech_y = gameJoinChooseTechY,
		.game_join_change_team_y = gameJoinChangeTeamY,
		.game_join_ready_y = gameJoinReadyY,
		.game_join_button_width = gameJoinButtonW,
		.game_join_button_height = lobby_screen_detail::kGameJoinButtonH,
		.show_game_tech_upper = showGameTechUpper,
		.game_tech = &gameTechState,
		.game_tech_back_x = rightUpperX + lobby_screen_detail::kGameTechBackPadLeft,
		.game_tech_back_y = gameTechBackY,
		.game_tech_back_width = gameTechBackW,
		.game_tech_back_height = lobby_screen_detail::kGameTechButtonH,
		.game_tech_peer_x = rightUpperX + lobby_screen_detail::kGameTechPeerColPadLeft,
		.game_tech_peer_y = gameTechPeerY,
		.game_tech_peer_width = gameTechPeerW,
		.game_tech_peer_row_gap = lobby_screen_detail::kGameTechPeerRowGap,
	};
	chromeFrame_.Build([&]() {
		                   return silencer::client_ui::lobby::LobbyChromeFrame(chromeProps);
	                   },
	                   layoutWidth,
	                   layoutHeight,
	                   interactions);

	if(ctx.renderdevice){
		ctx.renderdevice->BeginLobbyPanelBorderBlur(
			layoutWidth,
			layoutHeight,
			input.uiScale);
		lobby_screen_detail::AddPanelBorderBlur(
			ctx.renderdevice,
			SDL_Rect{ rootPadX, rootPadTop, bodyW, (int)titleBarH });
	}

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
		CLAY({ .id = CLAY_ID("LobbyTitleBarRetainedSlot"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(static_cast<float>(titleBarH)) },
		       } }) {}

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
		silencer::client_ui::LobbyModel lobby =
			silencer::client_ui::use_lobby(
				silencer::client_ui::MakeLobbyProvider(ctx, this));
		BuildLobbyMainArea(
			panels, ctx, lobby, bodyX, bodyY, bodyW, bodyH, regionGap, interactions);
		if(gameCreateActive){
			BuildGameCreatePreviewOverlay(gameCreateState, ctx);
		}
	}
}

void LobbyScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

void LobbyScreen::SetMapNameOverlay(const char * name)
{
	mapName = name ? std::string(name).substr(0, 25) : std::string();
}

namespace lobby_screen_flow_detail {

constexpr const char * kActionGoBack = "lobby.go_back";

MessageModal * ProgressModal(silencer::client_ui::lobby::GameCreatePanelState & state)
{
	return state.progressModal;
}

void DismissProgressModal(silencer::client_ui::lobby::GameCreatePanelState & state,
                          ScreenContext & ctx)
{
	if(!state.progressModal) return;
	silencer::client_ui::use_navigation().pop_top();
	state.progressModal = nullptr;
}

}  // namespace lobby_screen_flow_detail

void LobbyScreen::Tick(ScreenContext & ctx)
{
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx, this));

	if(lobby.session.disconnect_lobby_if_needed()){
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<LobbyConnectScreen>());
		return;
	}

	// Chrome Go Back -- flag was set by a typed button intent on the previous
	// frame. Consume it before pumping anything else.
	if(goBackClicked){
		goBackClicked = false;
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<MainMenuScreen>());
		return;
	}

	silencer::client_ui::lobby::CharacterPanelTick(characterState, lobby.character);
	if(characterState.newCharacterRequested){
		characterState.newCharacterRequested = false;
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<CharacterCreateScreen>());
		return;
	}
	silencer::client_ui::lobby::ChatPanelTick(chatState, lobby.chat);

	if(!gameCreateActive && !gameJoinActive && !gameTechActive){
		const silencer::client_ui::lobby::GameSelectPanelTickResult selected =
			silencer::client_ui::lobby::GameSelectPanelTick(
				gameSelectState, lobby);
		if(selected.show_create){
			ShowGameCreate(ctx);
		}
	}
	if(gameCreateActive){
		silencer::client_ui::lobby::GameCreatePanelTick(
			gameCreateState, ctx, lobby);
	}
	if(gameJoinActive){
		const silencer::client_ui::lobby::GameJoinPanelTickResult joined =
			silencer::client_ui::lobby::GameJoinPanelTick(
				gameJoinState, lobby);
		if(joined.show_tech){
			ShowGameTech(ctx);
		}
	}
	if(gameTechActive){
		const silencer::client_ui::lobby::GameTechPanelTickResult tech =
			silencer::client_ui::lobby::GameTechPanelTick(
				gameTechState, lobby);
		if(tech.show_roster){
			ShowGameJoin(ctx);
		}
	}

	MessageModal * progress =
		lobby_screen_flow_detail::ProgressModal(gameCreateState);
	const silencer::client_ui::LobbySessionPumpResult session =
		lobby.session.pump(gameJoinActive || gameTechActive, progress != nullptr);
	if(session.lobby_disconnected){
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<LobbyConnectScreen>());
		return;
	}
	if(progress && !session.progress_text.empty()){
		progress->SetText(ctx, session.progress_text);
	}
	if(session.dismiss_progress){
		lobby_screen_flow_detail::DismissProgressModal(gameCreateState, ctx);
	}
	if(!session.message.empty()){
		lobby.modal.show_message(session.message.c_str());
	}
	if(session.show_game_join){
		ShowGameJoin(ctx);
		SetMapNameOverlay(session.map_name.c_str());
	}
	if(session.disconnected_from_game && !disconnectMessageOpen){
		disconnectMessageOpen = true;
		silencer::client_ui::use_navigation().push(
			std::make_unique<MessageModal>(
				"Disconnected from game",
				[]() {
					silencer::client_ui::use_navigation()
						.reset_to(std::make_unique<MainMenuScreen>());
				}));
	}
}

bool LobbyScreen::HandleBack(ScreenContext & ctx)
{
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx, this));
	if(gameJoinActive || gameTechActive){
		lobby.pregame.leave_joined_game();
		lobby.browser.mark_games_dirty();
		SetMapNameOverlay("");
		ShowGameSelect(ctx);
		return true;
	}
	if(gameCreateActive){
		lobby.browser.mark_games_dirty();
		ShowGameSelect(ctx);
		return true;
	}
	return false;
}

bool LobbyScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		if(HandleBack(ctx)) return true;
		goBackClicked = true;
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate &&
	   action.id == lobby_screen_flow_detail::kActionGoBack){
		goBackClicked = true;
		return true;
	}
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx, this));
	if(silencer::client_ui::lobby::CharacterPanelHandleUiIntent(
			characterState, lobby.character, action)){
		return true;
	}
	if(silencer::client_ui::lobby::ChatPanelHandleUiIntent(chatState, lobby.chat, action)){
		return true;
	}
	if(gameCreateActive){
		return silencer::client_ui::lobby::GameCreatePanelHandleUiIntent(gameCreateState, action);
	}
	if(gameJoinActive){
		return silencer::client_ui::lobby::GameJoinPanelHandleUiIntent(gameJoinState, action);
	}
	if(gameTechActive){
		return silencer::client_ui::lobby::GameTechPanelHandleUiIntent(gameTechState, action);
	}
	return silencer::client_ui::lobby::GameSelectPanelHandleUiIntent(gameSelectState, action);
}

const ::ui::DrawCommandList * LobbyScreen::RetainedDrawCommands() const
{
	return &chromeFrame_.Commands();
}
