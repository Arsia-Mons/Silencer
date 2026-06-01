#include "game_tech_panel.h"

#include "client/ui/hooks/use_lobby.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace silencer::client_ui::lobby {

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

void GameTechPanelTick(GameTechPanelState & state,
                       LobbyModel & lobby) {
	const LobbyPregameTechModel::Status status = lobby.pregame.tech.status();
	state.slotsLeftStr = status.slots_left;
	state.peerNameStrs = status.peer_names;
	state.grid = MakeGridState(lobby.pregame.tech.grid());
}

}  // namespace silencer::client_ui::lobby
