#include "runtime/UiInputRouter.h"

namespace silencer {
namespace ui {

UiInputRouter::UiInputRouter(UiInteractionRegistry& registry)
	: registry_(registry) {}

std::vector<UiAction> UiInputRouter::Route(const UiInputState& input) {
	for(const UiControlCommand& command : input.controlCommands){
		if(command.kind == UiControlCommandKind::PointerPress){
			registry_.FocusControlHovered(static_cast<float>(command.x), static_cast<float>(command.y));
		}else if(command.kind == UiControlCommandKind::PointerHover){
			registry_.FocusControlHovered(static_cast<float>(command.x), static_cast<float>(command.y));
		}else{
			registry_.QueueAction(command.action);
		}
	}

	for(const UiBindingInput& binding : input.bindingInputs){
		UiAction action;
		action.kind = UiActionKind::CaptureBinding;
		action.id = "ui.capture_binding";
		action.binding = binding;
		registry_.QueueAction(action);
	}

	if(input.pointer.wheelX != 0.0f || input.pointer.wheelY != 0.0f){
		UiAction action;
		action.kind = UiActionKind::Scroll;
		action.value = "wheel";
		// Positive scroll amounts always mean "advance downward through the
		// content", regardless of the physical input source.
		action.amount = -static_cast<int>(input.pointer.wheelY);
		registry_.QueueAction(action);
	}

	return registry_.DrainActions();
}

}  // namespace ui
}  // namespace silencer
