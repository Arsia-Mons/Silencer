#include "game_join_panel.h"

#include "client/ui/hud/HudPayloadArena.h"
#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "primitives/button.h"
#include "primitives/text.h"
#include "runtime/UiInteractionRegistry.h"

#include "lobby_screen.h"
#include "game.h"
#include "resources.h"
#include "screen_context.h"
#include "team.h"
#include "user.h"
#include "world.h"

using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextSize;

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

constexpr uint16_t kRosterPadLeft = 56;
constexpr uint16_t kRosterPadTop = 7;
constexpr int kRosterTeamStepY = 55;
constexpr int kRosterPeerStepY = 13;
constexpr int kRosterEmblemAnchorX = 22;
constexpr int kRosterEmblemAnchorY = 6;
constexpr int kRosterReadyAnchorX = 57;
constexpr int kRosterReadyAnchorY = 6;
constexpr int kRosterNameX = 75;
constexpr int kRosterTextY = 8;
constexpr uint8_t kRosterLevelColor = 170;

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

	state.rosterRows.clear();
	if(world.IsConnected()){
		const std::vector<Uint16> & teamIds = world.GetObjectsByType(ObjectTypes::TEAM);
		for(Uint16 teamId : teamIds){
			Team * team = static_cast<Team *>(world.GetObjectFromId(teamId));
			if(!team || team->numpeers == 0) continue;
			bool drewEmblem = false;
			for(int i = 0; i < team->numpeers; ++i){
				Peer * peer = world.GetPeer(team->peers[i]);
				if(!peer || peer->observer || peer->disconnected) continue;
				User * user = world.lobby.GetUserInfo(peer->accountid);
				if(!user || user->retrieving || !user->name[0]) continue;

				GameJoinRosterRow row;
				row.ready = peer->isready;
				row.agency = team->agency;
				row.teamNumber = team->number;
				row.peerSlot = static_cast<Uint8>(i);
				row.drawEmblem = !drewEmblem;
				row.name = peer->isbot ? std::string(user->name) + " [BOT]"
				                       : std::string(user->name);
				row.level = "L:" + std::to_string(user->agency[team->agency].level);
				state.rosterRows.push_back(row);
				drewEmblem = true;
			}
		}
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
	(void)interactions;

	if(state.rosterRows.empty()) return;

	CLAY({ .id = CLAY_ID("GameJoinRoster"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { game_join_panel_detail::kRosterPadLeft, 0,
	                        game_join_panel_detail::kRosterPadTop, 0 },
	       } }) {
		for(size_t i = 0; i < state.rosterRows.size(); ++i){
			const GameJoinRosterRow & row = state.rosterRows[i];
			const int rowYOffset = row.teamNumber * game_join_panel_detail::kRosterTeamStepY
			                     + row.peerSlot * game_join_panel_detail::kRosterPeerStepY;

			if(row.drawEmblem){
				unsigned int emblemW = 16;
				unsigned int emblemH = 16;
				int emblemOffsetX = 0;
				int emblemOffsetY = 0;
				if(181 < resources.spritewidth.size()
				   && row.agency < resources.spritewidth[181].size()
				   && row.agency < resources.spriteheight[181].size()){
					emblemW = resources.spritewidth[181][row.agency];
					emblemH = resources.spriteheight[181][row.agency];
				}
				if(181 < resources.spriteoffsetx.size()
				   && row.agency < resources.spriteoffsetx[181].size()
				   && row.agency < resources.spriteoffsety[181].size()){
					emblemOffsetX = resources.spriteoffsetx[181][row.agency];
					emblemOffsetY = resources.spriteoffsety[181][row.agency];
				}
				CLAY({ .id = CLAY_IDI("GameJoinRosterEmblemWrap", static_cast<int>(i)),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(emblemW)),
				                       CLAY_SIZING_FIXED(static_cast<float>(emblemH)) },
				       },
				       .floating = {
				           .offset = {
				               static_cast<float>(game_join_panel_detail::kRosterEmblemAnchorX - emblemOffsetX),
				               static_cast<float>(game_join_panel_detail::kRosterEmblemAnchorY
				                                  + row.teamNumber * game_join_panel_detail::kRosterTeamStepY
				                                  - emblemOffsetY),
				           },
				           .attachTo = CLAY_ATTACH_TO_PARENT,
				       },
				       .custom = {
				           .customData = silencer::client_ui::AllocSpriteCustomData({
				               181, row.agency, 0, 0, 0, 0, 0, 128, 0, 0, 0, 0,
				           }),
				       } }) {}
			}

			const Uint16 readyIndex = row.ready ? 18 : 19;
			unsigned int readyW = 8;
			unsigned int readyH = 8;
			int readyOffsetX = 0;
			int readyOffsetY = 0;
			if(7 < resources.spritewidth.size()
			   && readyIndex < resources.spritewidth[7].size()
			   && readyIndex < resources.spriteheight[7].size()){
				readyW = resources.spritewidth[7][readyIndex];
				readyH = resources.spriteheight[7][readyIndex];
			}
			if(7 < resources.spriteoffsetx.size()
			   && readyIndex < resources.spriteoffsetx[7].size()
			   && readyIndex < resources.spriteoffsety[7].size()){
				readyOffsetX = resources.spriteoffsetx[7][readyIndex];
				readyOffsetY = resources.spriteoffsety[7][readyIndex];
			}
			CLAY({ .id = CLAY_IDI("GameJoinRosterReadyWrap", static_cast<int>(i)),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(readyW)),
			                       CLAY_SIZING_FIXED(static_cast<float>(readyH)) },
			       },
			       .floating = {
			           .offset = {
			               static_cast<float>(game_join_panel_detail::kRosterReadyAnchorX - readyOffsetX),
			               static_cast<float>(game_join_panel_detail::kRosterReadyAnchorY + rowYOffset - readyOffsetY),
			           },
			           .attachTo = CLAY_ATTACH_TO_PARENT,
			       },
			       .image = { .imageData = silencer::clay_bridge::PackImage(7, readyIndex) } }) {}

			CLAY({ .id = CLAY_IDI("GameJoinRosterNameWrap", static_cast<int>(i)),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			       },
			       .floating = {
			           .offset = {
			               static_cast<float>(game_join_panel_detail::kRosterNameX),
			               static_cast<float>(game_join_panel_detail::kRosterTextY + rowYOffset),
			           },
			           .attachTo = CLAY_ATTACH_TO_PARENT,
			       } }) {
				Text(game_join_panel_detail::FromStd(row.name),
				     { .size = TextSize::Body });
			}

			if(!row.level.empty()){
				const int levelX =
					game_join_panel_detail::kRosterNameX
					+ static_cast<int>(row.name.size()) * 6
					+ 3;
				CLAY({ .id = CLAY_IDI("GameJoinRosterLevelWrap", static_cast<int>(i)),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
				       },
				       .floating = {
				           .offset = {
				               static_cast<float>(levelX),
				               static_cast<float>(game_join_panel_detail::kRosterTextY + rowYOffset),
				           },
				           .attachTo = CLAY_ATTACH_TO_PARENT,
				       } }) {
					Text(game_join_panel_detail::FromStd(row.level),
					     { .size = TextSize::Tiny,
					       .effect = TextEffect::LegacyPalette(
					           game_join_panel_detail::kRosterLevelColor) });
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
