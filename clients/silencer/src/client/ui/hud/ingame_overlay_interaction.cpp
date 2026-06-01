#include "client/ui/hud/ingame_overlay_interaction.h"

#include "client/ui/hooks/use_match.h"
#include "client/ui/retained/RetainedFrame.h"
#include "runtime/UiInteractionRegistry.h"

#include <cstring>

namespace silencer {
namespace client_ui {

namespace {

bool StartsWith(const std::string& value, const char * prefix) {
	return value.compare(0, std::strlen(prefix), prefix) == 0;
}

bool ApplyInGameOverlayIntent(
		const MatchModel& match,
		const silencer::ui::UiAction& intent,
		silencer::ui::UiInteractionRegistry& interactions) {
	if(match.chat.active() &&
	   (intent.id == "ingame.chat" || intent.id == "ingame.chat.channel")){
		if(intent.kind == silencer::ui::UiActionKind::SubmitText){
			match.chat.submit(intent.value);
		}else if(intent.kind == silencer::ui::UiActionKind::Cancel){
			match.chat.cancel();
		}else if((intent.kind == silencer::ui::UiActionKind::Navigate ||
		          intent.kind == silencer::ui::UiActionKind::Activate) &&
		         intent.id == "ingame.chat.channel"){
			match.chat.toggle_channel();
			interactions.FocusInteractableById("ingame.chat");
		}
		return true;
	}

	if(match.station.active() && StartsWith(intent.id, "ingame.buytech.row.")){
		if(intent.index >= 0){
			match.station.select_row(intent.index);
		}
		if(intent.kind == silencer::ui::UiActionKind::Select &&
		   intent.value != "focus_next" && intent.value != "focus_previous"){
			match.station.activate_selected();
		}
		return true;
	}

	if(match.station.active() && intent.kind == silencer::ui::UiActionKind::Cancel){
		match.station.close();
		return true;
	}

	return false;
}

}  // namespace

void ApplyInGameOverlayIntents(
		const MatchModel& match,
		const RetainedFrame * overlayFrame,
		const std::vector<silencer::ui::UiAction>& intents,
		silencer::ui::UiInteractionRegistry& interactions) {
	for(const silencer::ui::UiAction& intent : intents){
		if(overlayFrame && overlayFrame->HandleUiIntent(intent)){
			continue;
		}
		(void)ApplyInGameOverlayIntent(match, intent, interactions);
	}
}

void FocusSelectedInGameStationRow(
		const HudView& view,
		silencer::ui::UiInteractionRegistry& interactions) {
	if(!view.buyTech.visible) return;
	for(const BuyTechRowView& row : view.buyTech.rows){
		if(!row.selected) continue;
		interactions.FocusInteractableById(
			"ingame.buytech.row." + std::to_string(row.index));
		return;
	}
}

}  // namespace client_ui
}  // namespace silencer
