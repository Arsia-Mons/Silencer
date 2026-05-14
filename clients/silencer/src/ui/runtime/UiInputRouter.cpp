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

UiInputRouter::UiInputRouter(UiAutomationRegistry& registry)
	: registry_(registry) {}

std::vector<UiAction> UiInputRouter::Route(const UiInputState& input) {
	if(input.pointer.pressed){
		registry_.InvokeAt(
			static_cast<int>(input.pointer.x),
			static_cast<int>(input.pointer.y));
	}

	for(char ascii : input.textInput){
		registry_.DispatchTextInput(ascii);
	}

	if(input.pointer.wheelX != 0.0f || input.pointer.wheelY != 0.0f){
		UiAction action;
		action.kind = UiActionKind::Scroll;
		action.value = "wheel";
		action.amount = static_cast<int>(input.pointer.wheelY);
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
