#include "client/ui/hud/ingame_overlay_frame_state.h"

#include "client/ui/hooks/use_match.h"

namespace silencer {
namespace client_ui {

namespace {

const char * QuitPromptText() {
#ifdef OUYA
	return "Hit O To QUIT";
#else
	return "Hit Enter To Quit";
#endif
}

}  // namespace

InGameOverlayFrameState MakeInGameOverlayFrameState(
		const HudView& hudView,
		const MatchModel& match,
		int width,
		int height,
		Uint8 hudPhase) {
	const bool showQuitPrompt =
		hudView.quitState == 1 || hudView.quitState == 2;
	const bool showTopMessage =
		hudView.topMessage.topmessage_i > 0 && !hudView.topMessage.text.empty();
	const bool showMessage =
		hudView.message.message_i > 0 && !hudView.message.message.empty();
	const bool showStatusMessages = !hudView.statusMessages.empty();
	const bool showPlayerList = hudView.showPlayerList;
	const bool showBuyTech =
		hudView.buyTech.visible && !hudView.buyTech.rows.empty() &&
		hudView.buyTech.backgroundW > 0 && hudView.buyTech.backgroundH > 0;
	const bool showChat = hudView.chat.visible;
	const bool showHudStatus = hudView.status.visible;
	const bool showTeamStrip =
		hudView.localPlayer.valid && hudView.viewedPlayer.valid &&
		hudView.teamStrip.visible;
	const bool showReadouts = hudView.readouts.visible;
	const bool showSecretOverlay = hudView.secretOverlay.visible;
	const bool showSystemCameraFrames =
		hudView.localPlayer.valid &&
		(hudView.systemCameraFrames[0].visible ||
		 hudView.systemCameraFrames[1].visible);

	InGameOverlayFrameState state;
	state.visible =
		showQuitPrompt || showTopMessage || showMessage ||
		showStatusMessages || showPlayerList || showBuyTech || showChat ||
		showHudStatus || showTeamStrip || showReadouts || showSecretOverlay ||
		showSystemCameraFrames;
	state.focus_station_row = showBuyTech;
	if(!state.visible) return state;

	state.props = InGameOverlayFrameProps{
		.key = "ingame-overlay",
		.width = width,
		.height = height,
		.hud_phase = hudPhase,
		.show_quit_prompt = showQuitPrompt,
		.quit_prompt_text = QuitPromptText(),
		.show_top_message = showTopMessage,
		.top_message_text = hudView.topMessage.text.c_str(),
		.top_message_progress = hudView.topMessage.topmessage_i,
		.show_message = showMessage,
		.message = hudView.message,
		.show_status_messages = showStatusMessages,
		.status_messages = hudView.statusMessages.data(),
		.status_message_count =
			static_cast<int>(hudView.statusMessages.size()),
		.show_player_list = showPlayerList,
		.teams = hudView.teams.data(),
		.team_count = static_cast<int>(hudView.teams.size()),
		.show_buy_tech = showBuyTech,
		.buy_tech = hudView.buyTech,
		.show_chat = showChat,
		.chat = hudView.chat,
		.set_chat_draft = [match](const char * value) {
			match.chat.set_draft(value ? value : "");
		},
		.show_status = showHudStatus,
		.status = hudView.status,
		.show_team_strip = showTeamStrip,
		.team_strip = hudView.teamStrip,
		.show_readouts = showReadouts,
		.readouts = hudView.readouts,
		.show_secret_overlay = showSecretOverlay,
		.secret_overlay = hudView.secretOverlay,
		.show_system_camera_frames = showSystemCameraFrames,
		.system_camera_frames = hudView.systemCameraFrames,
		.system_camera_frame_count = 2,
	};
	return state;
}

}  // namespace client_ui
}  // namespace silencer
