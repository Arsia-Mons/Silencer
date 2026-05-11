#include "clay_game_join_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "primitives/bank_button.h"

#include "lobby_clay_screen.h"
#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "resources.h"

using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonOpts;
using silencer::ui::primitives::BankButtonVariant;

namespace silencer::ui::lobby_clay {

namespace {

// Legacy GameJoinPanel coordinates — copied verbatim from
// game_join_panel.cpp so the on-screen geometry matches one-for-one.
constexpr int kBtnTechX  = 242;
constexpr int kBtnTechY  = 68;
constexpr int kBtnTeamX  = 242;
constexpr int kBtnTeamY  = 100;
constexpr int kBtnReadyX = 242;
constexpr int kBtnReadyY = 160;

void OnReadyClicked(void * user) {
	auto * state = static_cast<GameJoinPanelState *>(user);
	if(state) state->readyClicked = true;
}

void OnTeamClicked(void * user) {
	auto * state = static_cast<GameJoinPanelState *>(user);
	if(state) state->teamClicked = true;
}

void OnTechClicked(void * user) {
	auto * state = static_cast<GameJoinPanelState *>(user);
	if(state) state->techClicked = true;
}

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

}  // namespace

void GameJoinPanelInit(GameJoinPanelState & state) {
	state = GameJoinPanelState{};
}

void GameJoinPanelTick(GameJoinPanelState & state,
                       World & world,
                       ScreenContext & ctx,
                       LobbyClayScreen & owner) {
	// Per-frame Ready-button label. Mirrors GameJoinPanel::Tick's
	// `if(world.gameplaystate == INLOBBY)` block. The blocked check routes
	// through `owner` because World's peerlist/localpeerid/
	// AllPeersDownloadedMap are private and only friended classes can read
	// them — namespace-scope functions can't.
	if(owner.JoinPanelInLobby(world)){
		state.readyLabel = owner.JoinPanelReadyBlocked(world) ? "Waiting..." : "Ready";
	}else{
		state.readyLabel = "Ready";
	}

	if(state.techClicked){
		state.techClicked = false;
		owner.ShowGameTech(ctx);
		return;
	}
	if(state.readyClicked){
		state.readyClicked = false;
		owner.JoinPanelSendReady(world);
	}
	if(state.teamClicked){
		state.teamClicked = false;
		owner.JoinPanelChangeTeam(world);
	}
}

void BuildGameJoinPanelTree(GameJoinPanelState & state,
                            Resources & resources) {
	// Right border chrome sprite (bank 7 idx 8) — same layout as the other
	// right-pane panels (legacy positions an Overlay at (0, 0) with the
	// renderer subtracting spriteoffsetx/y).
	const Uint16 borderW = resources.spritewidth[7][8];
	const Uint16 borderH = resources.spriteheight[7][8];
	const int borderX = 0 - resources.spriteoffsetx[7][8];
	const int borderY = 0 - resources.spriteoffsety[7][8];
	CLAY({ .id = CLAY_ID("GJoinRightBorder"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(borderW)),
	                       CLAY_SIZING_FIXED(static_cast<float>(borderH)) },
	       },
	       .image    = { .imageData = silencer::clay_bridge::PackImage(7, 8) },
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { static_cast<float>(borderX),
	                                   static_cast<float>(borderY) } } }) {}

	// Choose Tech button (top).
	const int techOffX = kBtnTechX - resources.spriteoffsetx[7][24];
	const int techOffY = kBtnTechY - resources.spriteoffsety[7][24];
	CLAY({ .id = CLAY_ID("GJoinBtnTechWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { (float)techOffX, (float)techOffY } } }) {
		BankButton(CLAY_STRING("Choose Tech"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnTechClicked,
		                             /*user*/       &state });
	}

	// Change Team button (middle).
	const int teamOffX = kBtnTeamX - resources.spriteoffsetx[7][24];
	const int teamOffY = kBtnTeamY - resources.spriteoffsety[7][24];
	CLAY({ .id = CLAY_ID("GJoinBtnTeamWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { (float)teamOffX, (float)teamOffY } } }) {
		BankButton(CLAY_STRING("Change Team"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnTeamClicked,
		                             /*user*/       &state });
	}

	// Ready / Waiting... button (bottom). Label is mutated each Tick.
	const int readyOffX = kBtnReadyX - resources.spriteoffsetx[7][24];
	const int readyOffY = kBtnReadyY - resources.spriteoffsety[7][24];
	CLAY({ .id = CLAY_ID("GJoinBtnReadyWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { (float)readyOffX, (float)readyOffY } } }) {
		BankButton(FromStd(state.readyLabel),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnReadyClicked,
		                             /*user*/       &state });
	}
}

}  // namespace silencer::ui::lobby_clay
