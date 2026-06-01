#pragma once

#include "client/ui/views/HudView.h"
#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct InGameOverlayFrameProps {
	const char * key = nullptr;
	int width = 0;
	int height = 0;
	Uint8 hud_phase = 0;
	bool show_quit_prompt = false;
	const char * quit_prompt_text = nullptr;
	bool show_top_message = false;
	const char * top_message_text = nullptr;
	int top_message_progress = 0;
	bool show_message = false;
	InGameMessageView message = {};
	bool show_status_messages = false;
	const InGameStatusLineView * status_messages = nullptr;
	int status_message_count = 0;
	bool show_player_list = false;
	const TeamHudView * teams = nullptr;
	int team_count = 0;
	bool show_buy_tech = false;
	BuyTechOverlayView buy_tech = {};
	bool show_chat = false;
	ChatOverlayView chat = {};
	bool show_readouts = false;
	HudReadoutsView readouts = {};
	bool show_status = false;
	HudStatusView status = {};
	bool show_team_strip = false;
	HudTeamStripView team_strip = {};
	bool show_secret_overlay = false;
	HudSecretOverlayView secret_overlay = {};
	bool show_system_camera_frames = false;
	const SystemCameraFrameView * system_camera_frames = nullptr;
	int system_camera_frame_count = 0;
};

::ui::UiElement InGameOverlayFrame(const InGameOverlayFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
