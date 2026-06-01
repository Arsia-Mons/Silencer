#include "game_tech_panel.h"

#include "client/ui/hooks/use_lobby.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

namespace silencer::client_ui::lobby {

namespace game_tech_panel_detail {

constexpr const char * kActionBack = "lobby.game_tech.back";
constexpr const char * kActionTogglePrefix = "lobby.game_tech.toggle.";
constexpr const char * kActionDescriptionPrefix = "lobby.game_tech.description.";

bool StartsWith(const std::string & value, const char * prefix) {
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

int SuffixInt(const std::string & value, const char * prefix) {
	if(!StartsWith(value, prefix)) return -1;
	return std::atoi(value.c_str() + std::strlen(prefix));
}

}  // namespace game_tech_panel_detail

void GameTechPanelInit(GameTechPanelState & state) {
	state = GameTechPanelState{};
}

GameTechGridState MakeGridState(const LobbyPregameTechModel::Grid& grid) {
	GameTechGridState out;
	out.visible = grid.visible;
	out.localLabelsVisible = grid.local_labels_visible;
	out.rows.reserve(grid.rows.size());
	for(const LobbyPregameTechModel::GridRow& source : grid.rows){
		GameTechGridRowState row;
		row.itemIndex = source.item_index;
		row.label = source.label;
		row.labelBrightness = source.label_brightness;
		for(size_t i = 0; i < row.cells.size(); ++i){
			const LobbyPregameTechModel::GridCell& cell = source.cells[i];
			row.cells[i] = GameTechGridCellState{
				.draw = cell.draw,
				.local = cell.local,
				.selected = cell.selected,
				.brightness = cell.brightness,
			};
		}
		out.rows.push_back(std::move(row));
	}
	return out;
}

GameTechPanelTickResult GameTechPanelTick(GameTechPanelState & state,
                                          LobbyModel & lobby) {
	GameTechPanelTickResult result;
	const LobbyPregameTechModel::Status status = lobby.pregame.tech.status();
	state.slotsLeftStr = status.slots_left;
	state.peerNameStrs = status.peer_names;
	state.grid = MakeGridState(lobby.pregame.tech.grid());

	if(state.descClickedItemIndex >= 0){
		const int idx = state.descClickedItemIndex;
		state.descClickedItemIndex = -1;
		const LobbyPregameTechModel::Description desc =
			lobby.pregame.tech.description(idx);
		if(!desc.name.empty()){
			state.techNameStr = desc.name;
			state.techDescLines = desc.lines;
		}
	}

	if(state.toggleClickedItemIndex >= 0){
		const int idx = state.toggleClickedItemIndex;
		state.toggleClickedItemIndex = -1;
		lobby.pregame.tech.toggle(idx);
	}

	if(state.backClicked){
		state.backClicked = false;
		result.show_roster = true;
		return result;
	}
	return result;
}

bool GameTechPanelHandleUiIntent(GameTechPanelState & state,
                                 const silencer::ui::UiAction & action) {
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == game_tech_panel_detail::kActionBack){
		state.backClicked = true;
		return true;
	}
	int index = game_tech_panel_detail::SuffixInt(action.id, game_tech_panel_detail::kActionTogglePrefix);
	if(index >= 0){
		state.toggleClickedItemIndex = index;
		return true;
	}
	index = game_tech_panel_detail::SuffixInt(action.id, game_tech_panel_detail::kActionDescriptionPrefix);
	if(index >= 0){
		state.descClickedItemIndex = index;
		return true;
	}
	return false;
}

}  // namespace silencer::client_ui::lobby
