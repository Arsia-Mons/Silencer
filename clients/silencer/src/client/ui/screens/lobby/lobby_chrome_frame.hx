#pragma once

#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {
namespace lobby {

struct GameSelectPanelState;

struct LobbyChromeFrameProps {
	const char * key = nullptr;
	const char * version = nullptr;
	const char * map_name = nullptr;
	bool show_map_name = false;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	int pad_x = 0;
	int row_gap = 0;
	bool show_game_select_create = false;
	int game_select_create_x = 0;
	int game_select_create_y = 0;
	int game_select_create_width = 0;
	int game_select_create_height = 0;
	bool show_game_select_spectate = false;
	bool show_game_select_join = false;
	int game_select_spectate_x = 0;
	int game_select_spectate_y = 0;
	int game_select_join_x = 0;
	int game_select_join_y = 0;
	int game_select_action_width = 0;
	int game_select_action_height = 0;
	bool show_game_select_tall = false;
	int game_select_tall_x = 0;
	int game_select_tall_y = 0;
	int game_select_tall_width = 0;
	int game_select_tall_height = 0;
	const GameSelectPanelState * game_select = nullptr;
};

::ui::UiElement LobbyChromeFrame(const LobbyChromeFrameProps& props);

}  // namespace lobby
}  // namespace client_ui
}  // namespace silencer
