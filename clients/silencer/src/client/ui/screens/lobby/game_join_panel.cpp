#include "game_join_panel.h"

#include "client/ui/hooks/use_app.h"
#include "client/ui/hooks/use_lobby.h"
#include "client/ui/hud/HudPayloadArena.h"
#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "primitives/text.h"
#include "runtime/UiInteractionRegistry.h"

#include <utility>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextSize;

namespace silencer::client_ui::lobby {

namespace game_join_panel_detail {

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

GameJoinPanelTickResult GameJoinPanelTick(GameJoinPanelState & state,
                                          LobbyModel & lobby) {
	GameJoinPanelTickResult result;
	if(lobby.pregame.in_lobby()){
		state.readyLabel = lobby.pregame.ready_blocked() ? "Waiting..." : "Ready";
	}else{
		state.readyLabel = "Ready";
	}

	state.rosterRows.clear();
	for(const LobbyPregameRosterRow& modelRow : lobby.pregame.roster()){
		GameJoinRosterRow row;
		row.ready = modelRow.ready;
		row.agency = modelRow.agency;
		row.teamNumber = modelRow.team_number;
		row.peerSlot = modelRow.peer_slot;
		row.drawEmblem = modelRow.draw_emblem;
		row.name = modelRow.name;
		row.level = modelRow.level;
		state.rosterRows.push_back(std::move(row));
	}

	if(state.techClicked){
		state.techClicked = false;
		lobby.pregame.tech.request_peer_list();
		result.show_tech = true;
		return result;
	}
	if(state.readyClicked){
		state.readyClicked = false;
		lobby.pregame.set_ready(true);
	}
	if(state.teamClicked){
		state.teamClicked = false;
		lobby.pregame.team.change();
	}
	return result;
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

void BuildGameJoinTallTree(GameJoinPanelState & state,
                           const silencer::client_ui::AppAssetsModel& assets,
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
				const silencer::client_ui::AppSpriteFrame emblem =
					assets.agency_emblem(row.agency);
				if(emblem.available){
					emblemW = static_cast<unsigned int>(emblem.width);
					emblemH = static_cast<unsigned int>(emblem.height);
					emblemOffsetX = emblem.offset_x;
					emblemOffsetY = emblem.offset_y;
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
			const silencer::client_ui::AppSpriteFrame ready =
				assets.ready_indicator(row.ready);
			if(ready.available){
				readyW = static_cast<unsigned int>(ready.width);
				readyH = static_cast<unsigned int>(ready.height);
				readyOffsetX = ready.offset_x;
				readyOffsetY = ready.offset_y;
			}
			CLAY({ .id = CLAY_IDI("GameJoinRosterReadyWrap", static_cast<int>(i)),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(readyW)),
			                       CLAY_SIZING_FIXED(static_cast<float>(readyH)) },
			       },
			       .image = { .imageData = silencer::clay_bridge::PackImage(7, readyIndex) },
			       .floating = {
			           .offset = {
			               static_cast<float>(game_join_panel_detail::kRosterReadyAnchorX - readyOffsetX),
			               static_cast<float>(game_join_panel_detail::kRosterReadyAnchorY + rowYOffset - readyOffsetY),
			           },
			           .attachTo = CLAY_ATTACH_TO_PARENT,
			       } }) {}

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
