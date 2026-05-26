#include "runtime/UiInputRouter.h"

namespace silencer {
namespace ui {

UiInputRouter::UiInputRouter(UiInteractionRegistry& registry)
	: registry_(registry) {}

UiActionList UiInputRouter::Route(const UiInputState& input) {
	bool hasControlPointerCommand = false;
	for(const UiControlCommand& command : input.controlCommands){
		if(command.kind == UiControlCommandKind::PointerPress){
			hasControlPointerCommand = true;
			registry_.PressAt(command.x, command.y);
		}else if(command.kind == UiControlCommandKind::PointerHover){
			hasControlPointerCommand = true;
			registry_.FocusControlHovered(static_cast<float>(command.x), static_cast<float>(command.y));
		}else{
			registry_.QueueAction(command.action);
		}
	}

	if(input.pointer.pressed && !hasControlPointerCommand){
		registry_.PressAt(
			static_cast<int>(input.pointer.x),
			static_cast<int>(input.pointer.y));
	}

	// Legacy pointer hover still updates registry focus when the focus runtime
	// did not own the frame's declared layout.
	if(!hasControlPointerCommand && input.pointer.moved){
		registry_.FocusHovered(input.pointer.x, input.pointer.y);
	}

	for(const UiBindingInput& binding : input.bindingInputs){
		UiAction action;
		action.kind = UiActionKind::CaptureBinding;
		action.id = "ui.capture_binding";
		action.binding = binding;
		registry_.QueueAction(action);
	}

	for(char ascii : input.textInput){
		registry_.DispatchTextInput(ascii);
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

	for(auto action : input.navActions){
		bool handled = false;
		if(action == UiNavAction::Backspace){
			handled = registry_.BackspaceFocusedText();
		}else if(action == UiNavAction::Confirm){
			handled = registry_.SubmitFocusedText();
		}else if(action == UiNavAction::Cancel){
			handled = registry_.CancelFocused();
			if(!handled){
				UiAction cancel;
				cancel.kind = UiActionKind::Cancel;
				cancel.id = "ui.cancel";
				cancel.value = "cancel";
				registry_.QueueAction(cancel);
				handled = true;
			}
		}
		(void)handled;
	}

	return registry_.DrainActions();
}

}  // namespace ui
}  // namespace silencer
