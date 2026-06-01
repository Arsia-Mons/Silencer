#include "game_select_panel.h"

#include "client/ui/hooks/use_lobby.h"

#include <cstring>
#include <string>
#include <utility>

namespace silencer::client_ui::lobby {

namespace game_select_panel_detail {

constexpr Uint16 kListH     = 265;
constexpr Uint8  kListLineH = 14;

constexpr const char * kActionCreate = "lobby.game_select.create";
constexpr const char * kActionJoin = "lobby.game_select.join";
constexpr const char * kActionSpectate = "lobby.game_select.spectate";
constexpr const char * kActionRowPrefix = "lobby.game_select.row";

bool StartsWith(const std::string & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

void RebuildRows(GameSelectPanelState & state,
                 const std::vector<LobbyBrowserGameRow>& rows) {
	Uint32 prevSelectedId = 0;
	if(state.selectedIndex >= 0 && state.selectedIndex < (int)state.rows.size()){
		prevSelectedId = state.rows[state.selectedIndex].gameid;
	}
	state.rows.clear();
	for(const LobbyBrowserGameRow& game : rows){
		GameSelectPanelState::Row r;
		r.name   = game.name;
		r.gameid = game.id;
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

Uint32 SelectedGameId(GameSelectPanelState & state) {
	if(state.selectedIndex < 0 || state.selectedIndex >= (int)state.rows.size()){
		return 0;
	}
	return state.rows[state.selectedIndex].gameid;
}

void RecomputeInfoBlock(GameSelectPanelState & state,
                        const LobbyBrowserGameInfo& info) {
	if(info.name.empty()){
		state.infoName.clear();
		state.infoMap.clear();
		state.infoSecurity.clear();
		state.infoCreator.clear();
		state.infoLimits.clear();
		state.joinVisible = false;
		state.spectateVisible = false;
		return;
	}
	state.infoName = info.name;
	state.infoMap = info.map;
	state.infoSecurity = info.security;
	state.infoCreator = info.creator;
	state.infoLimits = info.limits;
	state.joinVisible = info.join_visible;
	state.spectateVisible = info.spectate_visible;
}

}  // namespace game_select_panel_detail

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

GameSelectPanelTickResult GameSelectPanelTick(GameSelectPanelState & state,
                                              LobbyModel & lobby) {
	GameSelectPanelTickResult result;
	const LobbyBrowserRowsSnapshot rows = lobby.browser.refresh_rows();
	if(rows.rebuilt){
		game_select_panel_detail::RebuildRows(state, rows.rows);
	}

	if(state.rowClickedIndex >= 0){
		state.selectedIndex = state.rowClickedIndex;
		state.rowClickedIndex = -1;
	}

	const Uint32 selectedGameId = game_select_panel_detail::SelectedGameId(state);
	game_select_panel_detail::RecomputeInfoBlock(
		state,
		lobby.browser.info(selectedGameId));

	if(state.createClicked){
		state.createClicked = false;
		result.show_create = true;
		return result;
	}
	if(state.joinClicked){
		state.joinClicked = false;
		lobby.browser.join(selectedGameId);
	}
	if(state.spectateClicked){
		state.spectateClicked = false;
		lobby.browser.spectate(selectedGameId);
	}
	return result;
}

bool GameSelectPanelHandleUiIntent(GameSelectPanelState & state,
                                   const silencer::ui::UiAction & action) {
	if(action.kind == silencer::ui::UiActionKind::Activate){
		if(action.id == game_select_panel_detail::kActionCreate){
			state.createClicked = true;
			return true;
		}
		if(action.id == game_select_panel_detail::kActionJoin){
			state.joinClicked = true;
			return true;
		}
		if(action.id == game_select_panel_detail::kActionSpectate){
			state.spectateClicked = true;
			return true;
		}
	}
	if(action.kind == silencer::ui::UiActionKind::Select &&
	   game_select_panel_detail::StartsWith(action.id, game_select_panel_detail::kActionRowPrefix)){
		state.rowClickedIndex = action.index;
		return true;
	}
	return false;
}

}  // namespace silencer::client_ui::lobby
