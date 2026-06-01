#pragma once

#include "client/ui/hud/ingame_overlay_frame.h"
#include "client/ui/views/HudView.h"

namespace silencer {
namespace client_ui {

class MatchModel;

struct InGameOverlayFrameState {
	bool visible = false;
	bool focus_station_row = false;
	InGameOverlayFrameProps props = {};
};

InGameOverlayFrameState MakeInGameOverlayFrameState(
	const HudView& hudView,
	const MatchModel& match,
	int width,
	int height,
	Uint8 hudPhase);

}  // namespace client_ui
}  // namespace silencer
