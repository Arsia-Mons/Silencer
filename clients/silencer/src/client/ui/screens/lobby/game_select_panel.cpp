#include "game_select_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/scroll_list.h"

#include "lobby_screen.h"
#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "lobbygame.h"
#include "config.h"
#include "user.h"
#include "resources.h"
#include "password_modal.h"

#include <cstring>
#include <deque>
#include <list>
#include <memory>
#include <string>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextOpts;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonOpts;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::ScrollList;
using silencer::ui::primitives::ScrollListHandle;
using silencer::ui::primitives::ScrollListOpts;

namespace silencer::client_ui::lobby {

namespace {

// Legacy on-screen coords kept ONLY for inspector hit-rect registration —
// dispatch is label-based; the rect is a fallback for geometric hit-testing.
constexpr int    kListX        = 407;
constexpr int    kListY        = 89;
constexpr Uint16 kListW        = 214;
constexpr Uint16 kListH        = 265;
constexpr Uint8  kListLineH    = 14;
constexpr int    kInfoX        = 405;
constexpr int    kInfoNameY    = 358;
constexpr int    kInfoMapY     = 370;
constexpr int    kInfoSecY     = 382;
constexpr int    kInfoCreatorY = 394;
constexpr int    kInfoLimitsY  = 406;
constexpr int    kBtnJoinX     = 436;
constexpr int    kBtnJoinY     = 430;
constexpr int    kBtnSpectateX = 436;
constexpr int    kBtnSpectateY = 408;
constexpr int    kBtnCreateX   = 242;
constexpr int    kBtnCreateY   = 68;

constexpr Uint8  kScrollbarBank = 7;

// LobbyRightUpperBox interior layout knobs. Box at (238, 64, 160x121) with
// 1-px border. Create Game button at screen (242, 68) → padLeft=4, padTop=4.
constexpr uint16_t kUpperBtnPadLeft = 4;
constexpr uint16_t kUpperBtnPadTop  = 4;

// LobbyRightTallBox interior layout knobs. Box at (398, 64, 232x391) with
// 1-px border. Successive children stack TOP_TO_BOTTOM; per-row padTop
// reproduces legacy y-positions.
constexpr uint16_t kTallHeadingPadLeft = 7;   // screen x=405 - box x=398
constexpr uint16_t kTallHeadingPadTop  = 6;   // screen y=70 - box y=64
constexpr uint16_t kTallListPadLeft    = 9;   // screen x=407 - box x=398
constexpr uint16_t kTallListPadTop     = 4;   // 89 - (64 + 6 + 15)
constexpr uint16_t kTallInfoBlockPadTop  = 4; // 358 - (89 + 265)
constexpr uint16_t kTallInfoPadLeft      = 7; // screen x=405 - box x=398
constexpr uint16_t kTallInfoRowH         = 12;// 12-px stride between info rows
constexpr uint16_t kTallButtonPadLeft = 38;   // screen x=436 - box x=398
constexpr uint16_t kTallButtonRowH    = 21;
constexpr uint16_t kTallSpectatePadTop = 2;   // small gap below info block
constexpr uint16_t kTallJoinPadTop     = 1;   // 430 - 408 - 21 = 1

constexpr int kMaxRows = 256;
Clay_String g_itemSlab[kMaxRows];
constexpr const char * kActionCreate = "lobby.game_select.create";
constexpr const char * kActionJoin = "lobby.game_select.join";
constexpr const char * kActionSpectate = "lobby.game_select.spectate";
constexpr const char * kActionRowPrefix = "lobby.game_select.row";

bool StartsWith(const std::string & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

Clay_String StaticId(const char * s) {
	return Clay_String{ true, static_cast<int32_t>(strlen(s)), s };
}

void RebuildRows(GameSelectPanelState & state, World & world) {
	Uint32 prevSelectedId = 0;
	if(state.selectedIndex >= 0 && state.selectedIndex < (int)state.rows.size()){
		prevSelectedId = state.rows[state.selectedIndex].gameid;
	}
	state.rows.clear();
	for(LobbyGame * lg : world.lobby.games){
		if(!lg) continue;
		GameSelectPanelState::Row r;
		r.name   = lg->name;
		r.gameid = lg->id;
		state.rows.push_back(std::move(r));
	}
	state.selectedIndex = -1;
	if(prevSelectedId != 0){
		for(size_t i = 0; i < state.rows.size(); ++i){
			if(state.rows[i].gameid == prevSelectedId){
				state.selectedIndex = static_cast<int>(i);
				break;
			}
		}
	}
	const int visible = kListH / kListLineH;
	int maxScroll = static_cast<int>(state.rows.size()) - visible;
	if(maxScroll < 0) maxScroll = 0;
	if(state.scrollPos > maxScroll) state.scrollPos = static_cast<Uint16>(maxScroll);
}

LobbyGame * GetSelectedGame(GameSelectPanelState & state, World & world) {
	if(state.selectedIndex < 0 || state.selectedIndex >= (int)state.rows.size()){
		return nullptr;
	}
	return world.lobby.GetGameById(state.rows[state.selectedIndex].gameid);
}

void RecomputeInfoBlock(GameSelectPanelState & state, World & world) {
	LobbyGame * lobbygame = GetSelectedGame(state, world);
	if(!lobbygame){
		state.infoName.clear();
		state.infoMap.clear();
		state.infoSecurity.clear();
		state.infoCreator.clear();
		state.infoLimits.clear();
		state.joinVisible = false;
		state.spectateVisible = false;
		return;
	}
	state.infoName = lobbygame->name;

	state.infoMap = "Map: ";
	state.infoMap += lobbygame->mapname;

	const char * passwordlock = (strlen(lobbygame->password) > 0)
	                              ? "*PASSWORD LOCK*" : "";
	std::string security = "No";
	switch(lobbygame->securitylevel){
		case LobbyGame::SECLOW:    security = "Low"; break;
		case LobbyGame::SECMEDIUM: security = "Medium"; break;
		case LobbyGame::SECHIGH:   security = "High"; break;
	}
	state.infoSecurity = security + " Security";
	while(state.infoSecurity.length() < 21){
		state.infoSecurity += " ";
	}
	state.infoSecurity += passwordlock;

	User * creator = world.lobby.GetUserInfo(lobbygame->accountid);
	state.infoCreator = "Creator: ";
	if(creator) state.infoCreator += creator->name;

	const bool ingame = lobbygame->state == 1;
	if(!ingame){
		state.infoLimits =
			"MinLv:" + std::to_string(lobbygame->minlevel)
			+ " MaxLv:" + std::to_string(lobbygame->maxlevel)
			+ " MaxPl:" + std::to_string(lobbygame->maxplayers)
			+ " MaxTm:" + std::to_string(lobbygame->maxteams);
	}else{
		state.infoLimits.clear();
	}

	state.joinVisible = false;
	state.spectateVisible = false;
	if(!ingame && lobbygame->players < lobbygame->maxplayers){
		state.joinVisible = true;
	}else if(ingame && lobbygame->canrejoin){
		state.joinVisible = true;
	}
	if(ingame && lobbygame->spectatable){
		state.spectateVisible = true;
	}
}

void HandleJoinClick(GameSelectPanelState & state, World & world, ScreenContext & ctx) {
	LobbyGame * lobbygame = GetSelectedGame(state, world);
	if(!lobbygame){
		ctx.ShowMessage("No game selected");
		return;
	}
	if(!world.IsIdle()) return;
	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	bool canjoin = true;
	if(user){
		if(lobbygame->minlevel > user->agency[Config::GetInstance().defaultagency].level){
			canjoin = false;
			ctx.ShowMessage("Your player level is too low");
		}else if(lobbygame->maxlevel < user->agency[Config::GetInstance().defaultagency].level){
			canjoin = false;
			ctx.ShowMessage("Your player level is too high");
		}
	}
	if(!canjoin) return;
	ctx.game.currentlobbygameid = lobbygame->id;
	if(strlen(lobbygame->password) > 0 && lobbygame->accountid != world.lobby.accountid){
		Uint32 gameId = lobbygame->id;
		ctx.PushScreen(std::make_unique<PasswordModal>(
			[&ctx, gameId](const char * password){
				LobbyGame * lg = ctx.world.lobby.GetGameById(gameId);
				if(lg){
					char buf[64];
					std::strncpy(buf, password ? password : "", sizeof(buf) - 1);
					buf[sizeof(buf) - 1] = '\0';
					ctx.game.JoinGame(*lg, buf);
				}
			}));
	}else{
		ctx.game.JoinGame(*lobbygame);
	}
}

void HandleSpectateClick(GameSelectPanelState & state, World & world, ScreenContext & ctx) {
	LobbyGame * lobbygame = GetSelectedGame(state, world);
	if(!lobbygame){
		ctx.ShowMessage("No game selected");
		return;
	}
	if(!world.IsIdle()) return;
	ctx.game.currentlobbygameid = lobbygame->id;
	if(strlen(lobbygame->password) > 0 && lobbygame->accountid != world.lobby.accountid){
		Uint32 gameId = lobbygame->id;
		ctx.PushScreen(std::make_unique<PasswordModal>(
			[&ctx, gameId](const char * password){
				LobbyGame * lg = ctx.world.lobby.GetGameById(gameId);
				if(lg){
					char buf[64];
					std::strncpy(buf, password ? password : "", sizeof(buf) - 1);
					buf[sizeof(buf) - 1] = '\0';
					ctx.game.SpectateGame(*lg, buf);
				}
			}));
	}else{
		ctx.game.SpectateGame(*lobbygame);
	}
}

void RegisterButton(const char * label, const char * actionId, int x, int y) {
	silencer::ui::automation::Widget w;
	w.id = actionId;
	w.labelText = label;
	w.kind  = silencer::ui::automation::WidgetKind::Button;
	w.x = x; w.y = y; w.w = 156; w.h = 21;
	silencer::ui::automation::Register(w);
}

}  // namespace

void GameSelectPanelInit(GameSelectPanelState & state) {
	state.rows.clear();
	state.selectedIndex   = -1;
	state.scrollPos       = 0;
	state.joinClicked     = false;
	state.spectateClicked = false;
	state.createClicked   = false;
	state.rowClickedIndex = -1;
	state.infoName.clear();
	state.infoMap.clear();
	state.infoSecurity.clear();
	state.infoCreator.clear();
	state.infoLimits.clear();
	state.joinVisible     = false;
	state.spectateVisible = false;
}

void GameSelectPanelTick(GameSelectPanelState & state,
                         World & world,
                         ScreenContext & ctx,
                         LobbyScreen & owner) {
	if(!world.lobby.gamesprocessed){
		RebuildRows(state, world);
		world.lobby.gamesprocessed = true;
	}

	if(state.rowClickedIndex >= 0){
		state.selectedIndex = state.rowClickedIndex;
		state.rowClickedIndex = -1;
	}

	RecomputeInfoBlock(state, world);

	if(state.createClicked){
		state.createClicked = false;
		owner.ShowGameCreate(ctx);
		return;
	}
	if(state.joinClicked){
		state.joinClicked = false;
		HandleJoinClick(state, world, ctx);
	}
	if(state.spectateClicked){
		state.spectateClicked = false;
		HandleSpectateClick(state, world, ctx);
	}
}

bool GameSelectPanelHandleUiIntent(GameSelectPanelState & state,
                                   const silencer::ui::UiAction & action) {
	if(action.kind == silencer::ui::UiActionKind::Activate){
		if(action.id == kActionCreate){
			state.createClicked = true;
			return true;
		}
		if(action.id == kActionJoin){
			state.joinClicked = true;
			return true;
		}
		if(action.id == kActionSpectate){
			state.spectateClicked = true;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::Select &&
	   StartsWith(action.id, kActionRowPrefix)){
		state.rowClickedIndex = action.index;
		return true;
	}
	return false;
}

void BuildGameSelectUpperTree(GameSelectPanelState & state,
                              Resources & resources) {
	(void)resources;

	// Create Game button — single flex child of the Upper box.
	CLAY({ .id = CLAY_ID("GSelBtnCreateWrap"),
	       .layout = { .padding = { kUpperBtnPadLeft, 0,
	                                kUpperBtnPadTop,  0 } } }) {
		BankButton(CLAY_STRING("Create Game"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   kActionCreate });
	}
	RegisterButton("Create Game", kActionCreate, kBtnCreateX, kBtnCreateY);
}

void BuildGameSelectTallTree(GameSelectPanelState & state,
                             Resources & resources) {
	(void)resources;

	// "Active Games" heading at top of Tall box.
	CLAY({ .id = CLAY_ID("GSelHeaderWrap"),
	       .layout = { .padding = { kTallHeadingPadLeft, 0,
	                                kTallHeadingPadTop,  0 } } }) {
		BankText(CLAY_STRING("Active Games"),
		         BankTextVariant::Heading,
		         {});
	}

	// Games list ScrollList.
	const int rowCount = static_cast<int>(state.rows.size());
	const int slotCount = (rowCount < kMaxRows) ? rowCount : kMaxRows;
	for(int i = 0; i < slotCount; ++i){
		g_itemSlab[i] = FromStd(state.rows[i].name);
	}

	ScrollListOpts listOpts;
	listOpts.width          = kListW;
	listOpts.height         = kListH;
	listOpts.lineHeight     = kListLineH;
	listOpts.highlightColor = 180;
	listOpts.textVariant    = BankTextVariant::Body;
	listOpts.scrollbarBank  = kScrollbarBank;

	CLAY({ .id = CLAY_ID("GSelListWrap"),
	       .layout = { .padding = { kTallListPadLeft, 0,
	                                kTallListPadTop,  0 } } }) {
		ScrollList(CLAY_STRING("GSelList"),
		           g_itemSlab,
		           slotCount,
		           state.selectedIndex,
		           state.scrollPos,
		           listOpts,
		           ScrollListHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   kActionRowPrefix });
	}

	for(int i = 0; i < slotCount; ++i){
		silencer::ui::automation::Widget w;
		w.id = std::string(kActionRowPrefix) + "." + std::to_string(i);
		w.labelText = state.rows[i].name;
		w.kind  = silencer::ui::automation::WidgetKind::ListRow;
		w.x = kListX; w.y = kListY + i * kListLineH;
		w.w = kListW; w.h = kListLineH;
		w.index = i;
		w.selected   = (state.selectedIndex == i);
		silencer::ui::automation::Register(w);
	}

	// Info-block group: 5 fixed-height row slots stacked TOP_TO_BOTTOM.
	// Each slot is height=12 so the layout doesn't reflow when text appears
	// or disappears. The first slot pads down from the list bottom.
	CLAY({ .id = CLAY_ID("GSelInfoBlock"),
	       .layout = {
	           .padding = { kTallInfoPadLeft, 0,
	                        kTallInfoBlockPadTop, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		const struct { const std::string * txt; const char * id; } kInfoRows[5] = {
			{ &state.infoName,     "GSelInfoName" },
			{ &state.infoMap,      "GSelInfoMap" },
			{ &state.infoSecurity, "GSelInfoSec" },
			{ &state.infoCreator,  "GSelInfoCreator" },
			{ &state.infoLimits,   "GSelInfoLimits" },
		};
		for(int i = 0; i < 5; ++i){
			CLAY({ .id = CLAY_SID(StaticId(kInfoRows[i].id)),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(kTallInfoRowH) },
			       } }) {
				if(!kInfoRows[i].txt->empty()){
					BankText(FromStd(*kInfoRows[i].txt),
					         BankTextVariant::Body,
					         {});
				}
			}
		}
	}

	// Spectate button — fixed-height slot so the Join slot below sits at a
	// stable y regardless of visibility.
	CLAY({ .id = CLAY_ID("GSelBtnSpectateWrap"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_FIXED(kTallButtonRowH) },
	           .padding = { kTallButtonPadLeft, 0,
	                        kTallSpectatePadTop, 0 },
	       } }) {
		if(state.spectateVisible){
			BankButton(CLAY_STRING("Spectate"),
			           BankButtonVariant::Chrome,
			           BankButtonOpts{},
			           BankButtonHandle{ /*hoveredOut*/ nullptr,
			                             /*actionId*/   kActionSpectate });
		}
	}
	if(state.spectateVisible){
		RegisterButton("Spectate", kActionSpectate, kBtnSpectateX, kBtnSpectateY);
	}

	// Join button.
	CLAY({ .id = CLAY_ID("GSelBtnJoinWrap"),
	       .layout = {
	           .padding = { kTallButtonPadLeft, 0,
	                        kTallJoinPadTop, 0 },
	       } }) {
		if(state.joinVisible){
			BankButton(CLAY_STRING("Join Game"),
			           BankButtonVariant::Chrome,
			           BankButtonOpts{},
			           BankButtonHandle{ /*hoveredOut*/ nullptr,
			                             /*actionId*/   kActionJoin });
		}
	}
	if(state.joinVisible){
		RegisterButton("Join Game", kActionJoin, kBtnJoinX, kBtnJoinY);
	}
}

}  // namespace silencer::client_ui::lobby
