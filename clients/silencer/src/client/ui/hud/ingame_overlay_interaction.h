#pragma once

#include "client/ui/views/HudView.h"
#include "ui/runtime/UiActionQueue.h"

#include <vector>

namespace silencer {
namespace ui {
class UiInteractionRegistry;
}
namespace client_ui {

class MatchModel;
class RetainedFrame;

void ApplyInGameOverlayIntents(
	const MatchModel& match,
	const RetainedFrame * overlayFrame,
	const std::vector<silencer::ui::UiAction>& intents,
	silencer::ui::UiInteractionRegistry& interactions);

void FocusSelectedInGameStationRow(
	const HudView& view,
	silencer::ui::UiInteractionRegistry& interactions);

}  // namespace client_ui
}  // namespace silencer
