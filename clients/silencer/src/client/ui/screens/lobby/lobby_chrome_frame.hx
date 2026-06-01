#pragma once

#include "ui/runtime/element.h"

#include <cstdint>
#include <functional>

namespace silencer {
namespace client_ui {
class AppAssetsModel;

namespace lobby {

struct GameJoinPanelState;
struct GameSelectPanelState;
struct GameTechPanelState;
struct GameCreatePanelState;
struct CharacterPanelState;
struct ChatPanelState;

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
	std::function<void()> go_back = {};
	bool show_body_chrome = false;
	int right_upper_x = 0;
	int right_upper_y = 0;
	int right_upper_width = 0;
	int right_upper_height = 0;
	int elbow_seam_x = 0;
	int elbow_seam_y = 0;
	int elbow_seam_width = 0;
	int elbow_seam_height = 0;
	int chat_tall_seam_x = 0;
	int chat_tall_seam_y = 0;
	int chat_tall_seam_width = 0;
	int chat_tall_seam_height = 0;
	int right_tall_x = 0;
	int right_tall_y = 0;
	int right_tall_width = 0;
	int right_tall_height = 0;
	bool show_character = false;
	const CharacterPanelState * character = nullptr;
	int character_x = 0;
	int character_y = 0;
	int character_width = 0;
	int character_height = 0;
	bool show_chat = false;
	const ChatPanelState * chat = nullptr;
	int chat_x = 0;
	int chat_y = 0;
	int chat_width = 0;
	int chat_height = 0;
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
	bool show_game_create_upper = false;
	const GameCreatePanelState * game_create = nullptr;
	int game_create_upper_x = 0;
	int game_create_upper_y = 0;
	int game_create_upper_width = 0;
	int game_create_upper_height = 0;
	bool show_game_create_tall = false;
	int game_create_tall_x = 0;
	int game_create_tall_y = 0;
	int game_create_tall_width = 0;
	int game_create_tall_height = 0;
	bool show_game_create_preview = false;
	int game_create_preview_x = 0;
	int game_create_preview_y = 0;
	int game_create_preview_width = 0;
	int game_create_preview_height = 0;
	int game_create_preview_line_height = 0;
	int game_create_preview_gap = 0;
	int game_create_preview_bitmap_width = 0;
	int game_create_preview_bitmap_height = 0;
	const char * game_create_preview_name = nullptr;
	const char * game_create_preview_description = nullptr;
	const std::uint8_t * game_create_preview_pixels = nullptr;
	bool show_game_join_actions = false;
	const char * game_join_ready_label = nullptr;
	int game_join_button_x = 0;
	int game_join_choose_tech_y = 0;
	int game_join_change_team_y = 0;
	int game_join_ready_y = 0;
	int game_join_button_width = 0;
	int game_join_button_height = 0;
	std::function<void()> game_join_choose_tech = {};
	std::function<void()> game_join_change_team = {};
	std::function<void()> game_join_ready = {};
	bool show_game_join_roster = false;
	const GameJoinPanelState * game_join = nullptr;
	const AppAssetsModel * app_assets = nullptr;
	int game_join_roster_x = 0;
	int game_join_roster_y = 0;
	int game_join_roster_width = 0;
	int game_join_roster_height = 0;
	bool show_game_tech_upper = false;
	const GameTechPanelState * game_tech = nullptr;
	int game_tech_back_x = 0;
	int game_tech_back_y = 0;
	int game_tech_back_width = 0;
	int game_tech_back_height = 0;
	int game_tech_peer_x = 0;
	int game_tech_peer_y = 0;
	int game_tech_peer_width = 0;
	int game_tech_peer_row_gap = 0;
	bool show_game_tech_tall = false;
	int game_tech_tall_x = 0;
	int game_tech_tall_y = 0;
	int game_tech_tall_width = 0;
	int game_tech_tall_height = 0;
};

::ui::UiElement LobbyChromeFrame(const LobbyChromeFrameProps& props);

}  // namespace lobby
}  // namespace client_ui
}  // namespace silencer
