#pragma once

#include "client/ui/views/HudView.h"
#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct InGameOverlayFrameProps {
	const char * key = nullptr;
	int width = 0;
	int height = 0;
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
};

::ui::UiElement InGameOverlayFrame(const InGameOverlayFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
