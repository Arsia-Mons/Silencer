#include "game_join_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_button.h"

#include "lobby_screen.h"
#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "resources.h"

using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonOpts;
using silencer::ui::primitives::BankButtonVariant;

namespace silencer::client_ui::lobby {

namespace {

// Legacy on-screen coords kept ONLY for inspector hit-rect registration —
// dispatch is label-based; the rect is a fallback for geometric hit-testing.
constexpr int kBtnTechX  = 242;
constexpr int kBtnTechY  = 68;
constexpr int kBtnTeamX  = 242;
constexpr int kBtnTeamY  = 100;
constexpr int kBtnReadyX = 242;
constexpr int kBtnReadyY = 160;
constexpr int kBtnW      = 156;
constexpr int kBtnH      = 21;

// LobbyRightUpperBox interior layout knobs. Box at (238, 64, 160, 121) with
// 1-px stroke → interior origin (239, 65). Buttons land at:
//   Tech:  (242, 68)  → padLeft=3, padTop=3
//   Team:  (242, 100) → padTop = 100 - (65+3+21) = 11
//   Ready: (242, 160) → padTop = 160 - (65+3+21+11+21) = 39
// All TOP_TO_BOTTOM wrappers carry padLeft=3 to align under the box's left
// stroke. Last button bottom y = 160+21 = 181, box bottom stroke at y=184 →
// 3 px clearance.
constexpr uint16_t kBtnPadLeft   = 3;
constexpr uint16_t kBtnTechPadTop  = 3;
constexpr uint16_t kBtnTeamPadTop  = 11;
constexpr uint16_t kBtnReadyPadTop = 39;

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

void RegisterButton(const char * label, int x, int y,
                    void (*onClick)(void *), void * user) {
	silencer::ui::automation::Widget w;
	w.label = label;
	w.kind  = silencer::ui::automation::WidgetKind::Button;
	w.x = x; w.y = y; w.w = kBtnW; w.h = kBtnH;
	w.onClick   = onClick;
	w.clickUser = user;
	silencer::ui::automation::Register(w);
}

}  // namespace

void GameJoinPanelInit(GameJoinPanelState & state) {
	state = GameJoinPanelState{};
}

void GameJoinPanelTick(GameJoinPanelState & state,
                       World & world,
                       ScreenContext & ctx,
                       LobbyScreen & owner) {
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

void BuildGameJoinUpperTree(GameJoinPanelState & state,
                            Resources & resources) {
	(void)resources;

	// Choose Tech (top button).
	CLAY({ .id = CLAY_ID("GJoinBtnTechWrap"),
	       .layout = { .padding = { kBtnPadLeft, 0, kBtnTechPadTop, 0 } } }) {
		BankButton(CLAY_STRING("Choose Tech"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnTechClicked,
		                             /*user*/       &state });
	}
	RegisterButton("Choose Tech", kBtnTechX, kBtnTechY, &OnTechClicked, &state);

	// Change Team (middle button).
	CLAY({ .id = CLAY_ID("GJoinBtnTeamWrap"),
	       .layout = { .padding = { kBtnPadLeft, 0, kBtnTeamPadTop, 0 } } }) {
		BankButton(CLAY_STRING("Change Team"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnTeamClicked,
		                             /*user*/       &state });
	}
	RegisterButton("Change Team", kBtnTeamX, kBtnTeamY, &OnTeamClicked, &state);

	// Ready / Waiting... (bottom button). Label flips per Tick.
	CLAY({ .id = CLAY_ID("GJoinBtnReadyWrap"),
	       .layout = { .padding = { kBtnPadLeft, 0, kBtnReadyPadTop, 0 } } }) {
		BankButton(FromStd(state.readyLabel),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*onClick*/    &OnReadyClicked,
		                             /*user*/       &state });
	}
	{
		silencer::ui::automation::Widget w;
		w.label = state.readyLabel.c_str();
		w.kind  = silencer::ui::automation::WidgetKind::Button;
		w.x = kBtnReadyX; w.y = kBtnReadyY; w.w = kBtnW; w.h = kBtnH;
		w.onClick = &OnReadyClicked; w.clickUser = &state;
		silencer::ui::automation::Register(w);
	}
}

void BuildGameJoinTallTree(GameJoinPanelState & state,
                           Resources & resources) {
	// GameJoin has no tall-pane content — the legacy panel only emitted the
	// 3 stacked buttons in the upper area. The LobbyRightTallBox renders as
	// empty chrome (just the 1-px stroke) when GameJoin is active.
	(void)state;
	(void)resources;
}

}  // namespace silencer::client_ui::lobby
