#include "game_select_panel.h"

#include "client/ui/hooks/use_lobby.h"

#include <string>
#include <utility>

namespace silencer::client_ui::lobby {

namespace game_select_panel_detail {

constexpr Uint16 kListH     = 265;
constexpr Uint8  kListLineH = 14;

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

Uint32 SelectedGameId(const GameSelectPanelState & state) {
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
	state.infoName.clear();
	state.infoMap.clear();
	state.infoSecurity.clear();
	state.infoCreator.clear();
	state.infoLimits.clear();
	state.joinVisible     = false;
	state.spectateVisible = false;
}

void GameSelectPanelTick(GameSelectPanelState & state,
                         LobbyModel & lobby) {
	const LobbyBrowserRowsSnapshot rows = lobby.browser.refresh_rows();
	if(rows.rebuilt){
		game_select_panel_detail::RebuildRows(state, rows.rows);
	}

	const Uint32 selectedGameId = game_select_panel_detail::SelectedGameId(state);
	game_select_panel_detail::RecomputeInfoBlock(
		state,
		lobby.browser.info(selectedGameId));
}

void GameSelectPanelSelect(GameSelectPanelState & state,
                           int index) {
	if(index < 0 || index >= static_cast<int>(state.rows.size())){
		return;
	}
	state.selectedIndex = index;
}

bool GameSelectPanelCanJoin(const GameSelectPanelState & state) {
	return state.joinVisible;
}

bool GameSelectPanelCanSpectate(const GameSelectPanelState & state) {
	return state.spectateVisible;
}

void GameSelectPanelJoin(const GameSelectPanelState & state,
                         const LobbyModel & lobby) {
	lobby.browser.join(game_select_panel_detail::SelectedGameId(state));
}

void GameSelectPanelSpectate(const GameSelectPanelState & state,
                             const LobbyModel & lobby) {
	lobby.browser.spectate(game_select_panel_detail::SelectedGameId(state));
}

}  // namespace silencer::client_ui::lobby
