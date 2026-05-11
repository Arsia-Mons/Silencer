#include "clay_game_select_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "primitives/bank_text.h"
#include "primitives/bank_button.h"
#include "primitives/scroll_list.h"

#include "lobby_clay_screen.h"
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

namespace silencer::ui::lobby_clay {

namespace {

// Legacy GameSelectPanel coordinates — copied verbatim from
// game_select_panel.cpp so the on-screen geometry matches one-for-one.
constexpr int    kListX        = 407;
constexpr int    kListY        = 89;
constexpr Uint16 kListW        = 214;
constexpr Uint16 kListH        = 265;
constexpr Uint8  kListLineH    = 14;
constexpr int    kHeaderX      = 405;
constexpr int    kHeaderY      = 70;
constexpr int    kInfoX        = 405;
// Legacy y-coords (uid→y): NAME=1@358, MAP=2@370, INFO=3@382 ("<Sec> Security
// <spaces>*PASSWORD LOCK*"), CREATOR=4@394, LIMITS=5@406 ("MinLv:.. MaxTm:..").
// Yes — the legacy enum names INFO/LIMITS are inverted relative to which row
// they actually populate. We follow the y-coord positioning, not the names.
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

// Per-frame Clay_String slab holding pointers into state.rows[].name. The
// std::strings are pointer-stable across BuildGameSelectPanelTree's call
// (no mutations during layout), so the slab can hand out raw c_str()s
// without copying.
constexpr int kMaxRows = 256;
Clay_String g_itemSlab[kMaxRows];

// Click adapters — three button slots + state pointer for the row select
// callback. Allocated as file-scope arrays overwritten in place each frame
// (same pattern as character panel's 5-element adapter array).
GameSelectPanelState * g_state = nullptr;

void OnRowSelected(void * user, int index) {
	auto * state = static_cast<GameSelectPanelState *>(user);
	if(state){
		state->rowClickedIndex = index;
	}
}

void OnJoinClicked(void * user) {
	auto * state = static_cast<GameSelectPanelState *>(user);
	if(state) state->joinClicked = true;
}

void OnSpectateClicked(void * user) {
	auto * state = static_cast<GameSelectPanelState *>(user);
	if(state) state->spectateClicked = true;
}

void OnCreateClicked(void * user) {
	auto * state = static_cast<GameSelectPanelState *>(user);
	if(state) state->createClicked = true;
}

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

// Rebuild rows from world.lobby.games. Mirrors the legacy "delete missing
// ids, append unknown ids" two-pass loop, but here we just rebuild from
// scratch each time gamesprocessed flips — selectedIndex tracks the
// previously-selected gameid across rebuilds so the user's selection is
// preserved when the lobby pushes an update.
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
	// Restore selection by gameid.
	state.selectedIndex = -1;
	if(prevSelectedId != 0){
		for(size_t i = 0; i < state.rows.size(); ++i){
			if(state.rows[i].gameid == prevSelectedId){
				state.selectedIndex = static_cast<int>(i);
				break;
			}
		}
	}
	// Clamp scroll.
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

// Mirrors the legacy "Join" case in GameSelectPanel::Tick. Returns false
// when join was blocked by a level check (caller already showed the
// message); true on a normal dispatch or no-op.
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
                         LobbyClayScreen & owner) {
	if(!world.lobby.gamesprocessed){
		RebuildRows(state, world);
		world.lobby.gamesprocessed = true;
	}

	// Apply any row click from last frame's Clay layout.
	if(state.rowClickedIndex >= 0){
		state.selectedIndex = state.rowClickedIndex;
		state.rowClickedIndex = -1;
	}

	RecomputeInfoBlock(state, world);

	if(state.createClicked){
		state.createClicked = false;
		owner.ShowGameCreate(ctx);
		return;  // Build was torn down — don't touch state below.
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

void BuildGameSelectPanelTree(GameSelectPanelState & state,
                              Resources & resources) {
	g_state = &state;

	// Right border chrome (bank 7 idx 8). Legacy creates this as an Overlay
	// at (0, 0); the renderer subtracts spriteoffsetx/y before BlitSurface.
	// Mirror by floating at (-spriteoffsetx, -spriteoffsety) so the Clay
	// bridge's natural top-left blit lands at the same pixels.
	const Uint16 borderW = resources.spritewidth[7][8];
	const Uint16 borderH = resources.spriteheight[7][8];
	const int borderX = 0 - resources.spriteoffsetx[7][8];
	const int borderY = 0 - resources.spriteoffsety[7][8];
	CLAY({ .id = CLAY_ID("GSelRightBorder"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(borderW)),
	                       CLAY_SIZING_FIXED(static_cast<float>(borderH)) },
	       },
	       .image    = { .imageData = silencer::clay_bridge::PackImage(7, 8) },
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { static_cast<float>(borderX),
	                                   static_cast<float>(borderY) } } }) {}

	// "Active Games" header at (405, 70), bank 134 / w8.
	CLAY({ .id = CLAY_ID("GSelHeaderWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kHeaderX, kHeaderY } } }) {
		BankText(CLAY_STRING("Active Games"),
		         BankTextVariant::Heading,
		         {});
	}

	// Games list ScrollList at (407, 89) size 214 x 265, lineheight 14.
	// Defaults match the legacy SelectBox + ScrollBar (bank 7 idx 9 track /
	// idx 10 thumb).
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

	CLAY({ .id = CLAY_ID("GSelListWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kListX, kListY } } }) {
		ScrollList(CLAY_STRING("GSelList"),
		           g_itemSlab,
		           slotCount,
		           state.selectedIndex,
		           state.scrollPos,
		           listOpts,
		           ScrollListHandle{ /*hoveredOut*/ nullptr,
		                             /*onSelect*/   &OnRowSelected,
		                             /*user*/       &state });
	}

	// Info-block text lines at (405, 358/370/382/394/406), bank 133 / w6.
	const struct { int y; const std::string * txt; const char * id; } kInfoRows[5] = {
		{ kInfoNameY,    &state.infoName,     "GSelInfoName" },
		{ kInfoMapY,     &state.infoMap,      "GSelInfoMap" },
		{ kInfoSecY,     &state.infoSecurity, "GSelInfoSec" },
		{ kInfoCreatorY, &state.infoCreator,  "GSelInfoCreator" },
		{ kInfoLimitsY,  &state.infoLimits,   "GSelInfoLimits" },
	};
	for(int i = 0; i < 5; ++i){
		if(kInfoRows[i].txt->empty()) continue;
		Clay_String wrapId;
		wrapId.isStaticallyAllocated = true;
		wrapId.length = static_cast<int32_t>(strlen(kInfoRows[i].id));
		wrapId.chars  = kInfoRows[i].id;
		CLAY({ .id = CLAY_SID(wrapId),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { kInfoX, (float)kInfoRows[i].y } } }) {
			BankText(FromStd(*kInfoRows[i].txt),
			         BankTextVariant::Body,
			         {});
		}
	}

	// Create Game button at (242, 68) — always visible.
	const int createOffX = kBtnCreateX - resources.spriteoffsetx[7][24];
	const int createOffY = kBtnCreateY - resources.spriteoffsety[7][24];
	CLAY({ .id = CLAY_ID("GSelBtnCreateWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { (float)createOffX, (float)createOffY } } }) {
		BankButton(CLAY_STRING("Create Game"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnCreateClicked,
		                             /*user*/       &state });
	}

	// Spectate button at (436, 408) — visible only when spectatable.
	if(state.spectateVisible){
		const int spectateOffX = kBtnSpectateX - resources.spriteoffsetx[7][24];
		const int spectateOffY = kBtnSpectateY - resources.spriteoffsety[7][24];
		CLAY({ .id = CLAY_ID("GSelBtnSpectateWrap"),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { (float)spectateOffX, (float)spectateOffY } } }) {
			BankButton(CLAY_STRING("Spectate"),
			           BankButtonVariant::Chrome,
			           BankButtonOpts{},
			           BankButtonHandle{ /*hoveredOut*/ nullptr,
			                             /*onClick*/    &OnSpectateClicked,
			                             /*user*/       &state });
		}
	}

	// Join button at (436, 430) — visible when joinable.
	if(state.joinVisible){
		const int joinOffX = kBtnJoinX - resources.spriteoffsetx[7][24];
		const int joinOffY = kBtnJoinY - resources.spriteoffsety[7][24];
		CLAY({ .id = CLAY_ID("GSelBtnJoinWrap"),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { (float)joinOffX, (float)joinOffY } } }) {
			BankButton(CLAY_STRING("Join Game"),
			           BankButtonVariant::Chrome,
			           BankButtonOpts{},
			           BankButtonHandle{ /*hoveredOut*/ nullptr,
			                             /*onClick*/    &OnJoinClicked,
			                             /*user*/       &state });
		}
	}
}

}  // namespace silencer::ui::lobby_clay
