#include "game_join_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"

#include "lobby_screen.h"
#include "screen_context.h"
#include "game.h"
#include "world.h"
#include "resources.h"

using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

namespace silencer::client_ui::lobby {

namespace game_join_panel_detail {

// Upper stepped-pane slot interior layout knobs. The padding preserves the
// legacy three-button vertical rhythm inside the shallow top shelf.
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
		Button(CLAY_STRING("GameJoinChooseTechButton"), CLAY_STRING("Choose Tech"),
		           ButtonOpts{ .variant = ButtonVariant::Chrome,
		                       .size = ButtonSize::Compact },
		           ButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   game_join_panel_detail::kActionTech,
		                             /*interactions*/ &interactions });
	}

	// Change Team (middle button).
	CLAY({ .id = CLAY_ID("GJoinBtnTeamWrap"),
	       .layout = { .padding = { game_join_panel_detail::kBtnPadLeft, 0, game_join_panel_detail::kBtnTeamPadTop, 0 } } }) {
		Button(CLAY_STRING("GameJoinChangeTeamButton"), CLAY_STRING("Change Team"),
		           ButtonOpts{ .variant = ButtonVariant::Chrome,
		                       .size = ButtonSize::Compact },
		           ButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   game_join_panel_detail::kActionTeam,
		                             /*interactions*/ &interactions });
	}

	// Ready / Waiting... (bottom button). Label flips per Tick.
	CLAY({ .id = CLAY_ID("GJoinBtnReadyWrap"),
	       .layout = { .padding = { game_join_panel_detail::kBtnPadLeft, 0, game_join_panel_detail::kBtnReadyPadTop, 0 } } }) {
		Button(CLAY_STRING("GameJoinReadyButton"), game_join_panel_detail::FromStd(state.readyLabel),
		           ButtonOpts{ .variant = ButtonVariant::Chrome,
		                       .size = ButtonSize::Compact },
		           ButtonHandle{ /*hoveredOut*/ nullptr,
		                             /*actionId*/   game_join_panel_detail::kActionReady,
		                             /*interactions*/ &interactions });
	}
}

void BuildGameJoinTallTree(GameJoinPanelState & state,
                           Resources & resources,
                           silencer::ui::UiInteractionRegistry& interactions) {
	// GameJoin has no tall-slot content — the legacy panel only emitted the
	// three stacked buttons in the upper shelf. The LobbyRightTallBox renders
	// as empty chrome when GameJoin is active.
	(void)state;
	(void)resources;
	(void)interactions;
}

}  // namespace silencer::client_ui::lobby
