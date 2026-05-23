#include "ui_interaction_json.h"

#include "runtime/UiInteractionRegistry.h"

#include <string>
#include <utility>

namespace ControlDispatch {

nlohmann::json InspectInteractionsToJson(
	const silencer::ui::UiInteractionRegistry& interactions) {
	nlohmann::json widgets = nlohmann::json::array();
	for(const auto & cw : interactions.Interactables()){
		nlohmann::json w;
		w["source"] = "clay";
		if(!cw.id.empty()) w["id"] = cw.id;
		w["x"] = cw.x; w["y"] = cw.y;
		w["w"] = cw.w; w["h"] = cw.h;
		if(silencer::ui::UiInteractableLabel(cw))
			w["label"] = silencer::ui::UiInteractableLabel(cw);
		if(cw.uid >= 0) w["uid"] = cw.uid;
		using K = silencer::ui::UiInteractableKind;
		switch(cw.kind){
			case K::Button:    w["kind"] = "button"; break;
			case K::Toggle:
				w["kind"] = "toggle";
				w["selected"] = cw.selected;
				break;
			case K::TextInput:
				w["kind"] = "textinput";
				w["password"] = cw.isPassword;
				w["text"] = cw.isPassword
					? std::string(cw.value.size(), '*')
					: cw.value;
				w["maxchars"] = cw.maxLength;
				break;
			case K::ListRow:
				w["kind"] = "listrow";
				w["row_index"] = cw.index;
				w["selected"] = cw.selected;
				break;
		}
		widgets.push_back(std::move(w));
	}
	nlohmann::json elements = nlohmann::json::array();
	for(const auto & element : interactions.Elements()){
		if(!element.id.empty() && interactions.FindInteractableById(element.id)){
			continue;
		}
		nlohmann::json e;
		e["source"] = "clay";
		if(!element.id.empty()) e["id"] = element.id;
		if(!element.label.empty()) e["label"] = element.label;
		if(!element.value.empty()) e["value"] = element.value;
		e["x"] = element.bounds.x;
		e["y"] = element.bounds.y;
		e["w"] = element.bounds.width;
		e["h"] = element.bounds.height;
		e["enabled"] = element.enabled;
		e["focused"] = element.focused;
		e["selected"] = element.selected;
		using EK = silencer::ui::UiElementKind;
		switch(element.kind){
			case EK::Container: e["kind"] = "container"; break;
			case EK::Button: e["kind"] = "button"; break;
			case EK::Text: e["kind"] = "text"; break;
			case EK::TextField: e["kind"] = "textfield"; break;
			case EK::ListItem: e["kind"] = "listitem"; break;
			case EK::Tab: e["kind"] = "tab"; break;
			case EK::Slider: e["kind"] = "slider"; break;
			case EK::Progress: e["kind"] = "progress"; break;
		}
		elements.push_back(std::move(e));
	}

	nlohmann::json r;
	r["widgets"] = widgets;
	r["elements"] = elements;
	r["interface_id"] = 0;
	return r;
}

}  // namespace ControlDispatch
