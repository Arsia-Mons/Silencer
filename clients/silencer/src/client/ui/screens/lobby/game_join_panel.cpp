#include "game_join_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
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

namespace game_join_panel_detail {

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
constexpr const char * kActionTech = "lobby.game_join.choose_tech";
constexpr const char * kActionTeam = "lobby.game_join.change_team";
constexpr const char * kActionReady = "lobby.game_join.ready";

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

void RegisterButton(silencer::ui::UiInteractionRegistry& interactions,
                    const char * label,
                    const char * actionId,
                    int x,
                    int y) {
	silencer::ui::UiInteractable w;
	w.id = actionId;
	w.labelText = label;
	w.kind  = silencer::ui::UiInteractableKind::Button;
	w.x = x; w.y = y; w.w = kBtnW; w.h = kBtnH;
	interactions.RegisterInteractable(w);
}

}  // namespace game_join_panel_detail

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

bool GameJoinPanelHandleUiIntent(GameJoinPanelState & state,
                                 const silencer::ui::UiAction & action) {
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == game_join_panel_detail::kActionTech){
		state.techClicked = true;
		return true;
	}
	if(action.id == game_join_panel_detail::kActionTeam){
		state.teamClicked = true;
		return true;
	}
	if(action.id == game_join_panel_detail::kActionReady){
		state.readyClicked = true;
		return true;
	}
	return false;
}

void BuildGameJoinUpperTree(GameJoinPanelState & state,
                            Resources & resources,
                            silencer::ui::UiInteractionRegistry& interactions) {
	(void)resources;

	// Choose Tech (top button).
	CLAY({ .id = CLAY_ID("GJoinBtnTechWrap"),
	       .layout = { .padding = { game_join_panel_detail::kBtnPadLeft, 0, game_join_panel_detail::kBtnTechPadTop, 0 } } }) {
		BankButton(CLAY_STRING("Choose Tech"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   game_join_panel_detail::kActionTech,
		                             /*interactions*/ &interactions });
	}
	game_join_panel_detail::RegisterButton(interactions, "Choose Tech", game_join_panel_detail::kActionTech, game_join_panel_detail::kBtnTechX, game_join_panel_detail::kBtnTechY);

	// Change Team (middle button).
	CLAY({ .id = CLAY_ID("GJoinBtnTeamWrap"),
	       .layout = { .padding = { game_join_panel_detail::kBtnPadLeft, 0, game_join_panel_detail::kBtnTeamPadTop, 0 } } }) {
		BankButton(CLAY_STRING("Change Team"),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   game_join_panel_detail::kActionTeam,
		                             /*interactions*/ &interactions });
	}
	game_join_panel_detail::RegisterButton(interactions, "Change Team", game_join_panel_detail::kActionTeam, game_join_panel_detail::kBtnTeamX, game_join_panel_detail::kBtnTeamY);

	// Ready / Waiting... (bottom button). Label flips per Tick.
	CLAY({ .id = CLAY_ID("GJoinBtnReadyWrap"),
	       .layout = { .padding = { game_join_panel_detail::kBtnPadLeft, 0, game_join_panel_detail::kBtnReadyPadTop, 0 } } }) {
		BankButton(game_join_panel_detail::FromStd(state.readyLabel),
		           BankButtonVariant::Chrome,
		           BankButtonOpts{},
		           BankButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   game_join_panel_detail::kActionReady,
		                             /*interactions*/ &interactions });
	}
	{
		silencer::ui::UiInteractable w;
		w.id = game_join_panel_detail::kActionReady;
		w.labelText = state.readyLabel;
		w.kind  = silencer::ui::UiInteractableKind::Button;
		w.x = game_join_panel_detail::kBtnReadyX; w.y = game_join_panel_detail::kBtnReadyY; w.w = game_join_panel_detail::kBtnW; w.h = game_join_panel_detail::kBtnH;
		interactions.RegisterInteractable(w);
	}
}

void BuildGameJoinTallTree(GameJoinPanelState & state,
                           Resources & resources,
                           silencer::ui::UiInteractionRegistry& interactions) {
	// GameJoin has no tall-pane content — the legacy panel only emitted the
	// 3 stacked buttons in the upper area. The LobbyRightTallBox renders as
	// empty chrome (just the 1-px stroke) when GameJoin is active.
	(void)state;
	(void)resources;
	(void)interactions;
}

}  // namespace silencer::client_ui::lobby
