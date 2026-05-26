#include "runtime/UiInputRouter.h"

namespace silencer {
namespace ui {

namespace {

bool MovesFocusForward(UiNavAction action) {
	return action == UiNavAction::FocusNext ||
	       action == UiNavAction::NextSection;
}

bool MovesFocusBackward(UiNavAction action) {
	return action == UiNavAction::FocusPrevious ||
	       action == UiNavAction::PreviousSection;
}

bool MovesSpatially(UiNavAction action) {
	return action == UiNavAction::Up ||
	       action == UiNavAction::Down ||
	       action == UiNavAction::Left ||
	       action == UiNavAction::Right;
}

}  // namespace

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

	// Mouse hover and keyboard navigation share one focus state: moving the
	// pointer over an interactable focuses it, so the keyboard-focused element
	// stops being highlighted instead of leaving two lit at once.
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
		}else if(MovesFocusForward(action)){
			handled = registry_.FocusNextInteractive();
		}else if(MovesFocusBackward(action)){
			handled = registry_.FocusPreviousInteractive();
		}else if(MovesSpatially(action)){
			handled = registry_.FocusDirectional(action);
		}else if(action == UiNavAction::Confirm){
			handled = registry_.SubmitFocusedText();
			if(!handled) handled = registry_.ActivateFocused();
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
