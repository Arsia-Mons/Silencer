#include "lobby_screen.h"

#include "character_create_screen.h"
#include "game_create_panel.h"
#include "client/ui/hooks/use_app.h"
#include "client/ui/hooks/use_lobby.h"
#include "client/ui/hooks/use_navigation.h"
#include "client/ui/screens/lobby/lobby_chrome_frame.h"
#include "lobby_connect_screen.h"
#include "main_menu_screen.h"

#include "screen_context.h"
#include "game.h"
#include "message_modal.h"
#include "renderdevice.h"
#include "renderer.h"
#include "ui/runtime/UiInputState.h"

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
constexpr int kLegacyBodyW = 620;
constexpr int kLegacyBodyH = 391;
constexpr int kLegacyTopRowContentW = 378;
constexpr int kLegacyCharacterW = 218;
constexpr int kLegacyTallW = 232;
constexpr int kMinTallW = 170;
constexpr int kMaxTallW = 320;
constexpr int kMinChatW = 220;
constexpr int kMinUpperW = 120;
constexpr int kMinCharacterW = 140;
constexpr int kMaxCharacterW = 300;
constexpr int kMinLowerLeftH = 88;
constexpr Uint8 kBorderTop = 1 << 0;
constexpr Uint8 kBorderRight = 1 << 1;
constexpr Uint8 kBorderBottom = 1 << 2;
constexpr Uint8 kBorderLeft = 1 << 3;
constexpr Uint8 kBorderAll =
	kBorderTop | kBorderRight | kBorderBottom | kBorderLeft;

struct LobbyMainAreaLayout {
	int regionGap = 10;
	int characterW = 218;
	int rightUpperW = 0;
	int upperH = 121;
	int rightTallW = 232;
	int rightTallH = 391;
	int topRowW = 0;
	int chatW = 0;
	int chatH = 260;
};

int ClampInt(int value, int lo, int hi) {
	if(value < lo) return lo;
	if(value > hi) return hi;
	return value;
}

	int RoundRatio(int actual,
	               int numerator,
	               int denominator) {
	if(denominator <= 0) return 0;
	return static_cast<int>(
		(static_cast<long long>(actual) * numerator + denominator / 2) / denominator);
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

LobbyMainAreaLayout ResolveLobbyMainAreaLayout(int bodyW,
                                               int bodyH,
                                               int regionGap) {
	LobbyMainAreaLayout out;
	out.regionGap = regionGap;
	out.upperH = ClampInt(RoundRatio(bodyH, 121, kLegacyBodyH), 84, 156);
	int maxUpperH = std::max(0, bodyH - regionGap - kMinLowerLeftH);
	if(maxUpperH > 0 && out.upperH > maxUpperH){
		out.upperH = maxUpperH;
	}

	int desiredRightTallW =
		ClampInt(RoundRatio(bodyW, kLegacyTallW, kLegacyBodyW), kMinTallW, kMaxTallW);
	const int maxRightTallW = std::max(0, bodyW - kMinChatW);
	if(desiredRightTallW > maxRightTallW){
		desiredRightTallW = maxRightTallW;
	}
	if(maxRightTallW >= kMinTallW && desiredRightTallW < kMinTallW){
		desiredRightTallW = kMinTallW;
	}
	out.rightTallW = desiredRightTallW;
	out.rightTallH = std::max(0, bodyH);
	out.topRowW = std::max(0, bodyW - out.rightTallW);
	out.chatW = std::max(0, out.topRowW - regionGap);
	out.chatH = std::max(0, bodyH - out.upperH - regionGap);

	const int availableTopRowW = std::max(0, out.topRowW - regionGap);
	const int desiredCharacterW = ClampInt(
		RoundRatio(availableTopRowW, kLegacyCharacterW, kLegacyTopRowContentW),
		kMinCharacterW,
		kMaxCharacterW);
	const int maxCharacterW = std::max(0, availableTopRowW - kMinUpperW);
	if(maxCharacterW >= kMinCharacterW){
		out.characterW = ClampInt(desiredCharacterW, kMinCharacterW, maxCharacterW);
	}else{
		out.characterW = maxCharacterW;
	}
	out.rightUpperW = std::max(0, availableTopRowW - out.characterW);
	return out;
}

void AddPanelBorderBlur(RenderDevice * renderdevice, SDL_Rect rect) {
	if(!renderdevice || rect.w <= 0 || rect.h <= 0) return;
	renderdevice->AddLobbyPanelBorderBlurRect({ rect.x, rect.y, rect.w, 1 });
	renderdevice->AddLobbyPanelBorderBlurRect({ rect.x, rect.y + rect.h - 1, rect.w, 1 });
	renderdevice->AddLobbyPanelBorderBlurRect({ rect.x, rect.y, 1, rect.h });
	renderdevice->AddLobbyPanelBorderBlurRect({ rect.x + rect.w - 1, rect.y, 1, rect.h });
}

void AddPanelBorderBlur(RenderDevice * renderdevice,
                        int x,
                        int y,
                        int w,
                        int h,
                        Uint8 sides) {
	if(!renderdevice || w <= 0 || h <= 0) return;
	if(sides & kBorderTop){
		renderdevice->AddLobbyPanelBorderBlurRect({ x, y, w, 1 });
	}
	if(sides & kBorderBottom){
		renderdevice->AddLobbyPanelBorderBlurRect({ x, y + h - 1, w, 1 });
	}
	if(sides & kBorderLeft){
		renderdevice->AddLobbyPanelBorderBlurRect({ x, y, 1, h });
	}
	if(sides & kBorderRight){
		renderdevice->AddLobbyPanelBorderBlurRect({ x + w - 1, y, 1, h });
	}
}

void QueueLobbyPanelBorderBlurRects(RenderDevice * renderdevice,
                                    int bodyX,
                                    int bodyY,
                                    const LobbyMainAreaLayout & layout) {
	if(!renderdevice) return;
	const int topY = bodyY;
	const int lowerY = bodyY + layout.upperH + layout.regionGap;
	const int rightX = bodyX + layout.topRowW;
	const int characterX = bodyX;
	const int rightUpperX = bodyX + layout.characterW + layout.regionGap;
	const int seamX = bodyX + layout.topRowW - layout.regionGap;

	AddPanelBorderBlur(renderdevice,
	                   characterX, topY,
	                   layout.characterW, layout.upperH,
	                   kBorderAll);
	AddPanelBorderBlur(renderdevice,
	                   rightUpperX, topY,
	                   layout.rightUpperW, layout.upperH,
	                   static_cast<Uint8>(kBorderTop | kBorderBottom | kBorderLeft));
	AddPanelBorderBlur(renderdevice,
	                   seamX, bodyY + layout.upperH,
	                   layout.regionGap, layout.regionGap,
	                   kBorderRight);
	AddPanelBorderBlur(renderdevice,
	                   bodyX, lowerY,
	                   layout.chatW, layout.chatH,
	                   kBorderAll);
	AddPanelBorderBlur(renderdevice,
	                   seamX, lowerY,
	                   layout.regionGap, layout.chatH,
	                   kBorderRight);
	AddPanelBorderBlur(renderdevice,
	                   rightX, bodyY,
	                   layout.rightTallW, layout.rightTallH,
	                   static_cast<Uint8>(kBorderTop | kBorderBottom | kBorderRight));
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
	disconnectMessageOpen = false;

	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	silencer::client_ui::lobby::CharacterPanelInit(characterState, lobby.character);
	silencer::client_ui::lobby::ChatPanelInit(chatState);
	silencer::client_ui::lobby::GameSelectPanelInit(gameSelectState);
	silencer::client_ui::lobby::GameJoinPanelInit(gameJoinState);
	silencer::client_ui::lobby::GameTechPanelInit(gameTechState);
	rightPane = LobbyRightPane::GameSelect;
}

void LobbyScreen::ShowGameSelect()
{
	rightPane = LobbyRightPane::GameSelect;
}

void LobbyScreen::ShowGameCreate(const silencer::client_ui::LobbyModel & lobby)
{
	silencer::client_ui::lobby::GameCreatePanelInit(gameCreateState, lobby);
	rightPane = LobbyRightPane::GameCreate;
}

void LobbyScreen::ShowGameJoin()
{
	silencer::client_ui::lobby::GameJoinPanelInit(gameJoinState);
	rightPane = LobbyRightPane::GameJoin;
}

void LobbyScreen::ShowGameTech()
{
	silencer::client_ui::lobby::GameTechPanelInit(gameTechState);
	rightPane = LobbyRightPane::GameTech;
}

void LobbyScreen::BuildUi(ScreenContext & ctx, float frametime, const silencer::ui::UiInputState & input, Uint8, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	using namespace silencer::client_ui::lobby;

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
	const lobby_screen_detail::LobbyMainAreaLayout mainLayout =
		lobby_screen_detail::ResolveLobbyMainAreaLayout(bodyW, bodyH, regionGap);
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	const silencer::client_ui::AppModel app =
		silencer::client_ui::use_app(
			silencer::client_ui::MakeAppProvider(ctx));
	const bool isGameSelectPane = rightPane == LobbyRightPane::GameSelect;
	const bool isGameCreatePane = rightPane == LobbyRightPane::GameCreate;
	const bool isGameJoinPane = rightPane == LobbyRightPane::GameJoin;
	const bool isGameTechPane = rightPane == LobbyRightPane::GameTech;
	ChatPanelSyncLayout(
		chatState,
		static_cast<Uint16>(std::max(0, mainLayout.chatW)),
		static_cast<Uint16>(std::max(0, mainLayout.chatH)));
	if(isGameCreatePane){
		GameCreatePanelSyncOptionsLayout(
			gameCreateState,
			static_cast<Uint16>(std::max(0, mainLayout.rightUpperW)),
			static_cast<Uint16>(std::max(0, mainLayout.upperH)));
		GameCreatePanelSyncTallLayout(
			gameCreateState,
			input,
			app.audio,
			lobby,
			static_cast<Uint16>(std::max(0, mainLayout.rightTallW)),
			static_cast<Uint16>(std::max(0, mainLayout.rightTallH)),
			bodyX + mainLayout.topRowW,
			bodyY);
	}
	const GameCreatePreviewOverlayLayout gameCreatePreviewLayout =
		isGameCreatePane
			? ResolveGameCreatePreviewOverlayLayout(gameCreateState, input)
			: GameCreatePreviewOverlayLayout{};
	const int createButtonW = std::max(
		1,
		mainLayout.rightUpperW
			- lobby_screen_detail::kGameSelectCreatePadLeft
			- lobby_screen_detail::kGameSelectCreatePadRight);
	const bool showGameSelectCreate =
		isGameSelectPane &&
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
		isGameSelectPane &&
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
	const int lowerY = bodyY + mainLayout.upperH + mainLayout.regionGap;
	const int seamX = bodyX + mainLayout.topRowW - mainLayout.regionGap;
	const int rightTallX = bodyX + mainLayout.topRowW;
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
		isGameJoinPane &&
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
		isGameTechPane &&
		mainLayout.rightUpperW > lobby_screen_detail::kGameTechBackPadLeft
			+ lobby_screen_detail::kGameTechBackPadRight &&
		mainLayout.upperH > gameTechPeerY - bodyY + 11;
	const bool showGameCreateUpper =
		isGameCreatePane && mainLayout.rightUpperW > 0 && mainLayout.upperH > 0;
	const bool showGameCreateTall =
		isGameCreatePane && mainLayout.rightTallW > 0 && mainLayout.rightTallH > 0;
	const uint16_t titlePadX = static_cast<uint16_t>(
		lobby_screen_detail::ClampInt((layoutWidth * 5) / 640, 5, 10));
	const uint16_t titleRowGap = static_cast<uint16_t>(
		lobby_screen_detail::ClampInt((layoutWidth * 6) / 640, 4, 10));
	const bool showMapName = !mapName.empty() && layoutWidth >= 700;
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();

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
		.go_back = [navigation]() {
			navigation.reset_to(std::make_unique<MainMenuScreen>());
		},
		.show_body_chrome = bodyW > 0 && bodyH > 0,
		.right_upper_x = rightUpperX,
		.right_upper_y = bodyY,
		.right_upper_width = mainLayout.rightUpperW,
		.right_upper_height = mainLayout.upperH,
		.elbow_seam_x = seamX,
		.elbow_seam_y = bodyY + mainLayout.upperH,
		.elbow_seam_width = mainLayout.regionGap,
		.elbow_seam_height = mainLayout.regionGap,
		.chat_tall_seam_x = seamX,
		.chat_tall_seam_y = lowerY,
		.chat_tall_seam_width = mainLayout.regionGap,
		.chat_tall_seam_height = mainLayout.chatH,
		.right_tall_x = rightTallX,
		.right_tall_y = bodyY,
		.right_tall_width = mainLayout.rightTallW,
		.right_tall_height = mainLayout.rightTallH,
		.show_character = mainLayout.characterW > 0 && mainLayout.upperH > 0,
		.character = &characterState,
		.character_x = bodyX,
		.character_y = bodyY,
		.character_width = mainLayout.characterW,
		.character_height = mainLayout.upperH,
		.character_open_agents = [lobby, navigation]() {
			if(lobby.character.agent_selection_locked()) return;
			navigation.reset_to(std::make_unique<CharacterCreateScreen>());
		},
		.show_chat = mainLayout.chatW > 0 && mainLayout.chatH > 0,
		.chat = &chatState,
		.chat_x = bodyX,
		.chat_y = bodyY + mainLayout.upperH + mainLayout.regionGap,
		.chat_width = mainLayout.chatW,
		.chat_height = mainLayout.chatH,
		.chat_set_input = [this](const char * value) {
			silencer::client_ui::lobby::ChatPanelSetInput(chatState, value);
		},
		.chat_submit_input = [this, lobby](const char * value) {
			silencer::client_ui::lobby::ChatPanelSubmitInput(
				chatState, lobby.chat, value);
		},
		.show_game_select_create = showGameSelectCreate,
		.game_select_create_x = bodyX + mainLayout.characterW + mainLayout.regionGap
		                        + lobby_screen_detail::kGameSelectCreatePadLeft,
		.game_select_create_y = bodyY + lobby_screen_detail::kGameSelectCreatePadTop,
		.game_select_create_width = createButtonW,
		.game_select_create_height = lobby_screen_detail::kGameSelectCreateButtonH,
		.game_select_create = [this, lobby]() {
			ShowGameCreate(lobby);
		},
		.show_game_select_spectate =
			showGameSelectActions && GameSelectPanelCanSpectate(gameSelectState),
		.show_game_select_join =
			showGameSelectActions && GameSelectPanelCanJoin(gameSelectState),
		.game_select_spectate_x = actionButtonX,
		.game_select_spectate_y = spectateButtonY,
		.game_select_join_x = actionButtonX,
		.game_select_join_y = joinButtonY,
		.game_select_action_width = lobby_screen_detail::kGameSelectActionButtonW,
		.game_select_action_height = lobby_screen_detail::kGameSelectActionButtonH,
		.game_select_spectate = [this, lobby]() {
			GameSelectPanelSpectate(gameSelectState, lobby);
		},
		.game_select_join = [this, lobby]() {
			GameSelectPanelJoin(gameSelectState, lobby);
		},
		.show_game_select_tall = isGameSelectPane,
		.game_select_tall_x = rightTallX,
		.game_select_tall_y = bodyY,
		.game_select_tall_width = mainLayout.rightTallW,
		.game_select_tall_height = mainLayout.rightTallH,
		.game_select = &gameSelectState,
		.game_select_select = [this](int index) {
			GameSelectPanelSelect(gameSelectState, index);
		},
		.show_game_create_upper = showGameCreateUpper,
		.game_create = &gameCreateState,
		.game_create_upper_x = rightUpperX,
		.game_create_upper_y = bodyY,
		.game_create_upper_width = mainLayout.rightUpperW,
		.game_create_upper_height = mainLayout.upperH,
		.game_create_cycle_security = [this]() {
			silencer::client_ui::lobby::GameCreatePanelCycleSecurity(
				gameCreateState);
		},
		.game_create_toggle_spectatable = [this, lobby]() {
			silencer::client_ui::lobby::GameCreatePanelToggleSpectatable(
				gameCreateState, lobby);
		},
		.game_create_set_text = [this](
			silencer::client_ui::lobby::GameCreatePanelTextField field,
			const char * value) {
			silencer::client_ui::lobby::GameCreatePanelSetText(
				gameCreateState, field, value);
		},
		.game_create_scroll_options = [this](int amount) {
			silencer::client_ui::lobby::GameCreatePanelScrollOptions(
				gameCreateState, amount);
		},
		.show_game_create_tall = showGameCreateTall,
		.game_create_tall_x = rightTallX,
		.game_create_tall_y = bodyY,
		.game_create_tall_width = mainLayout.rightTallW,
		.game_create_tall_height = mainLayout.rightTallH,
		.game_create_select_map = [this, lobby](int index) {
			silencer::client_ui::lobby::GameCreatePanelSelectMap(
				gameCreateState, lobby, index);
		},
		.game_create_submit = [this, lobby]() {
			silencer::client_ui::lobby::GameCreatePanelSubmit(
				gameCreateState, lobby);
		},
		.show_game_create_preview = gameCreatePreviewLayout.visible,
		.game_create_preview_x = gameCreatePreviewLayout.x,
		.game_create_preview_y = gameCreatePreviewLayout.y,
		.game_create_preview_width = gameCreatePreviewLayout.width,
		.game_create_preview_height = gameCreatePreviewLayout.height,
		.game_create_preview_line_height = gameCreatePreviewLayout.lineHeight,
		.game_create_preview_gap = gameCreatePreviewLayout.gap,
		.game_create_preview_bitmap_width = gameCreatePreviewLayout.bitmapWidth,
		.game_create_preview_bitmap_height = gameCreatePreviewLayout.bitmapHeight,
		.game_create_preview_name = gameCreateState.hoverPreviewName.c_str(),
		.game_create_preview_description = gameCreateState.hoverPreviewDescription.c_str(),
		.game_create_preview_pixels = gameCreateState.hoverPreviewPixels.empty()
			? nullptr
			: gameCreateState.hoverPreviewPixels.data(),
		.show_game_join_actions = showGameJoinActions,
		.game_join_ready_label = gameJoinState.readyLabel.c_str(),
		.game_join_button_x = rightUpperX + lobby_screen_detail::kGameJoinButtonPadLeft,
		.game_join_choose_tech_y = gameJoinChooseTechY,
		.game_join_change_team_y = gameJoinChangeTeamY,
		.game_join_ready_y = gameJoinReadyY,
		.game_join_button_width = gameJoinButtonW,
		.game_join_button_height = lobby_screen_detail::kGameJoinButtonH,
		.game_join_choose_tech = [this, lobby]() {
			lobby.pregame.tech.request_peer_list();
			ShowGameTech();
		},
		.game_join_change_team = [lobby]() {
			lobby.pregame.team.change();
		},
		.game_join_ready = [lobby]() {
			lobby.pregame.set_ready(true);
		},
		.show_game_join_roster = isGameJoinPane,
		.game_join = &gameJoinState,
		.app_assets = &app.assets,
		.game_join_roster_x = rightTallX,
		.game_join_roster_y = bodyY,
		.game_join_roster_width = mainLayout.rightTallW,
		.game_join_roster_height = mainLayout.rightTallH,
		.show_game_tech_upper = showGameTechUpper,
		.game_tech = &gameTechState,
		.game_tech_back_x = rightUpperX + lobby_screen_detail::kGameTechBackPadLeft,
		.game_tech_back_y = gameTechBackY,
		.game_tech_back_width = gameTechBackW,
		.game_tech_back_height = lobby_screen_detail::kGameTechButtonH,
		.game_tech_back = [this]() {
			ShowGameJoin();
		},
		.game_tech_toggle = [lobby](int item_index) {
			lobby.pregame.tech.toggle(item_index);
		},
		.game_tech_describe = [this, lobby](int item_index) {
			GameTechPanelDescribe(gameTechState, lobby, item_index);
		},
		.game_tech_peer_x = rightUpperX + lobby_screen_detail::kGameTechPeerColPadLeft,
		.game_tech_peer_y = gameTechPeerY,
		.game_tech_peer_width = gameTechPeerW,
		.game_tech_peer_row_gap = lobby_screen_detail::kGameTechPeerRowGap,
		.show_game_tech_tall = isGameTechPane,
		.game_tech_tall_x = rightTallX,
		.game_tech_tall_y = bodyY,
		.game_tech_tall_width = mainLayout.rightTallW,
		.game_tech_tall_height = mainLayout.rightTallH,
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
		lobby_screen_detail::QueueLobbyPanelBorderBlurRects(
			ctx.renderdevice,
			bodyX,
			bodyY,
			mainLayout);
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

void LobbyScreen::Tick(ScreenContext & ctx)
{
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));

	if(lobby.session.disconnect_lobby_if_needed()){
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<LobbyConnectScreen>());
		return;
	}

	silencer::client_ui::lobby::CharacterPanelTick(characterState, lobby.character);
	silencer::client_ui::lobby::ChatPanelTick(chatState, lobby.chat);

	const bool isGameSelectPane = rightPane == LobbyRightPane::GameSelect;
	const bool isGameCreatePane = rightPane == LobbyRightPane::GameCreate;
	const bool isGameJoinPane = rightPane == LobbyRightPane::GameJoin;
	const bool isGameTechPane = rightPane == LobbyRightPane::GameTech;

	if(isGameSelectPane){
		silencer::client_ui::lobby::GameSelectPanelTick(
			gameSelectState, lobby);
	}
	if(isGameCreatePane){
		silencer::client_ui::lobby::GameCreatePanelTick(
			gameCreateState, lobby);
	}
	if(isGameJoinPane){
		silencer::client_ui::lobby::GameJoinPanelTick(
			gameJoinState, lobby);
	}
	if(isGameTechPane){
		silencer::client_ui::lobby::GameTechPanelTick(
			gameTechState, lobby);
	}

	MessageModal * progress =
		silencer::client_ui::lobby::GameCreatePanelProgressModal(gameCreateState);
	const silencer::client_ui::LobbySessionPumpResult session =
		lobby.session.pump(isGameJoinPane || isGameTechPane, progress != nullptr);
	if(session.lobby_disconnected){
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<LobbyConnectScreen>());
		return;
	}
	if(progress && !session.progress_text.empty()){
		progress->SetText(session.progress_text);
	}
	if(session.dismiss_progress){
		silencer::client_ui::lobby::GameCreatePanelDismissProgressModal(
			gameCreateState);
	}
	if(!session.message.empty()){
		lobby.modal.show_message(session.message.c_str());
	}
	if(session.show_game_join){
		ShowGameJoin();
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
			silencer::client_ui::MakeLobbyProvider(ctx));
	if(rightPane == LobbyRightPane::GameJoin ||
	   rightPane == LobbyRightPane::GameTech){
		lobby.pregame.leave_joined_game();
		lobby.browser.mark_games_dirty();
		SetMapNameOverlay("");
		ShowGameSelect();
		return true;
	}
	if(rightPane == LobbyRightPane::GameCreate){
		lobby.browser.mark_games_dirty();
		ShowGameSelect();
		return true;
	}
	return false;
}

bool LobbyScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		if(HandleBack(ctx)) return true;
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<MainMenuScreen>());
		return true;
	}
	if(chromeFrame_.HandleUiIntent(action)) return true;
	if(rightPane == LobbyRightPane::GameJoin){
		return false;
	}
	if(rightPane == LobbyRightPane::GameTech){
		return false;
	}
	return false;
}

const ::ui::DrawCommandList * LobbyScreen::RetainedDrawCommands() const
{
	return &chromeFrame_.Commands();
}
