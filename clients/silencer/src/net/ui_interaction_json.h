#ifndef SILENCER_NET_UI_INTERACTION_JSON_H
#define SILENCER_NET_UI_INTERACTION_JSON_H

#include <nlohmann/json.hpp>

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace ControlDispatch {

nlohmann::json InspectInteractionsToJson(
	const silencer::ui::UiInteractionRegistry& interactions);

}  // namespace ControlDispatch

#endif
