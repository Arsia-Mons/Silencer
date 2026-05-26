#include "doctest.h"

#include "client/ui/ClientUiInput.h"
#include "runtime/UiActionQueue.h"
#include "runtime/UiInteractionRegistry.h"

#include <array>
#include <string>
#include <utility>

namespace {

silencer::ui::UiAction MakeAction(int index) {
	silencer::ui::UiAction action;
	action.kind = silencer::ui::UiActionKind::Activate;
	action.id = "action." + std::to_string(index);
	action.index = index;
	return action;
}

}  // namespace

TEST_CASE("UiActionQueue uses bounded storage and drops overflow actions") {
	silencer::ui::UiActionQueue queue;

	for(int i = 0; i < silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS + 2; ++i){
		const bool pushed = queue.Push(MakeAction(i));
		CHECK(pushed == (i < silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS));
	}

	CHECK(queue.Count() == silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS);
	CHECK(queue.OverflowCount() == 2);

	silencer::ui::UiActionList actions = queue.Drain();
	REQUIRE(actions.size() == silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS);
	CHECK(actions.front().id == "action.0");
	CHECK(actions.back().id == "action.127");
	CHECK(queue.Empty());
	CHECK(queue.Count() == 0);

	CHECK(queue.Push(MakeAction(200)));
	actions = queue.Drain();
	REQUIRE(actions.size() == 1);
	CHECK(actions[0].id == "action.200");
	CHECK(queue.OverflowCount() == 2);
}

TEST_CASE("UiActionList uses bounded storage and drops overflow actions") {
	silencer::ui::UiActionList actions;

	for(int i = 0; i < silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS + 1; ++i){
		const bool pushed = actions.Push(MakeAction(i));
		CHECK(pushed == (i < silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS));
	}

	CHECK(actions.size() == silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS);
	CHECK(actions.OverflowCount() == 1);
	CHECK(actions.front().id == "action.0");
	CHECK(actions.back().id == "action.127");

	actions.Clear();
	CHECK(actions.empty());
	CHECK(actions.OverflowCount() == 1);
}

TEST_CASE("UiInteractionRegistry surfaces action queue overflow diagnostics") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	for(int i = 0; i < silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS + 1; ++i){
		const bool queued = registry.QueueAction(MakeAction(i));
		CHECK(queued == (i < silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS));
	}

	CHECK(registry.PendingActionCount() == silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS);
	CHECK(registry.ActionOverflowCount() == 1);

	silencer::ui::UiActionList actions = registry.DrainActions();
	REQUIRE(actions.size() == silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS);
	CHECK(actions.front().id == "action.0");
	CHECK(actions.back().id == "action.127");
	CHECK(registry.PendingActionCount() == 0);
	CHECK(registry.ActionOverflowCount() == 1);
}

TEST_CASE("UiInteractionRegistry bounds frame metadata registration") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	for(int i = 0; i < silencer::ui::UI_INTERACTION_MAX_REGISTERED_ELEMENTS + 2; ++i){
		silencer::ui::UiElementSnapshot element;
		element.id = "element." + std::to_string(i);
		const bool registered = registry.Register(std::move(element));
		CHECK(registered == (i < silencer::ui::UI_INTERACTION_MAX_REGISTERED_ELEMENTS));
	}

	auto elements = registry.Elements();
	CHECK(elements.size() == silencer::ui::UI_INTERACTION_MAX_REGISTERED_ELEMENTS);
	CHECK(registry.ElementOverflowCount() == 2);
	REQUIRE(!elements.empty());
	CHECK(elements[0].id == "element.0");
	CHECK(elements[elements.size() - 1].id == "element.511");

	registry.BeginFrame();
	CHECK(registry.Elements().empty());
	CHECK(registry.ElementOverflowCount() == 2);
	CHECK(registry.Register(silencer::ui::UiElementSnapshot{}));
	CHECK(registry.Elements().size() == 1);
}

TEST_CASE("UiInteractionRegistry bounds frame interactable registration") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	for(int i = 0; i < silencer::ui::UI_INTERACTION_MAX_INTERACTABLES + 1; ++i){
		silencer::ui::UiInteractable widget;
		widget.id = "button." + std::to_string(i);
		widget.labelText = "Button " + std::to_string(i);
		widget.kind = silencer::ui::UiInteractableKind::Button;
		const bool registered = registry.RegisterInteractable(std::move(widget));
		CHECK(registered == (i < silencer::ui::UI_INTERACTION_MAX_INTERACTABLES));
	}

	auto interactables = registry.Interactables();
	auto elements = registry.Elements();
	CHECK(interactables.size() == silencer::ui::UI_INTERACTION_MAX_INTERACTABLES);
	CHECK(elements.size() == silencer::ui::UI_INTERACTION_MAX_INTERACTABLES);
	CHECK(registry.InteractableOverflowCount() == 1);
	CHECK(registry.ElementOverflowCount() == 0);
	REQUIRE(!interactables.empty());
	CHECK(interactables[0].id == "button.0");
	CHECK(interactables[interactables.size() - 1].id == "button.255");
	CHECK(registry.FindById("button.255") != nullptr);
	CHECK(registry.FindById("button.256") == nullptr);
}

TEST_CASE("UiInputState bounds frame-local input buffers") {
	silencer::ui::UiInputState input;

	for(int i = 0; i < silencer::ui::UI_INPUT_MAX_TEXT_CHARS + 1; ++i){
		input.textInput.push_back('x');
	}
	for(int i = 0; i < silencer::ui::UI_INPUT_MAX_NAV_ACTIONS + 1; ++i){
		input.navActions.push_back(silencer::ui::UiNavAction::Down);
	}
	for(int i = 0; i < silencer::ui::UI_INPUT_MAX_BINDING_INPUTS + 1; ++i){
		silencer::ui::UiBindingInput binding;
		binding.code = i;
		input.bindingInputs.push_back(binding);
	}
	for(int i = 0; i < silencer::ui::UI_INPUT_MAX_CONTROL_COMMANDS + 1; ++i){
		silencer::ui::UiControlCommand command;
		command.action = MakeAction(i);
		input.controlCommands.push_back(command);
	}

	CHECK(input.textInput.size() == silencer::ui::UI_INPUT_MAX_TEXT_CHARS);
	CHECK(input.textInput.OverflowCount() == 1);
	CHECK(input.navActions.size() == silencer::ui::UI_INPUT_MAX_NAV_ACTIONS);
	CHECK(input.navActions.OverflowCount() == 1);
	CHECK(input.bindingInputs.size() == silencer::ui::UI_INPUT_MAX_BINDING_INPUTS);
	CHECK(input.bindingInputs.OverflowCount() == 1);
	CHECK(input.controlCommands.size() == silencer::ui::UI_INPUT_MAX_CONTROL_COMMANDS);
	CHECK(input.controlCommands.OverflowCount() == 1);

	input.textInput.clear();
	input.navActions.clear();
	input.bindingInputs.clear();
	input.controlCommands.clear();
	CHECK(input.textInput.empty());
	CHECK(input.navActions.empty());
	CHECK(input.bindingInputs.empty());
	CHECK(input.controlCommands.empty());
	CHECK(input.textInput.OverflowCount() == 1);
	CHECK(input.controlCommands.OverflowCount() == 1);
}

TEST_CASE("ClientUiInput builds frames from bounded input buffers") {
	silencer::client_ui::ClientUiInput input;

	for(int i = 0; i < silencer::ui::UI_INPUT_MAX_NAV_ACTIONS + 2; ++i){
		input.QueueNavAction(silencer::ui::UiNavAction::Confirm);
	}
	for(int i = 0; i < silencer::ui::UI_INPUT_MAX_TEXT_CHARS + 2; ++i){
		input.QueueTextInput('a');
	}

	silencer::ui::UiInputState frame = input.BuildFrame(640, 480, 1.0f, 1.0f / 60.0f);

	CHECK(frame.navActions.size() == silencer::ui::UI_INPUT_MAX_NAV_ACTIONS);
	CHECK(frame.navActions.OverflowCount() == 2);
	CHECK(frame.textInput.size() == silencer::ui::UI_INPUT_MAX_TEXT_CHARS);
	CHECK(frame.textInput.OverflowCount() == 2);

	input.EndFrame();
	frame = input.BuildFrame(640, 480, 1.0f, 1.0f / 60.0f);
	CHECK(frame.navActions.empty());
	CHECK(frame.textInput.empty());
}

TEST_CASE("ClientUiInput bounds gamepad axis edge storage") {
	silencer::client_ui::ClientUiInput input;
	std::array<int16_t, silencer::client_ui::CLIENT_UI_INPUT_MAX_GAMEPAD_AXES + 2> axes = {};

	input.CaptureGamepadBindingEdges(0, axes.data(), static_cast<int>(axes.size()), 1000);
	CHECK(input.GamepadBindingAxisOverflowCount() == 1);

	axes[0] = 12000;
	axes[silencer::client_ui::CLIENT_UI_INPUT_MAX_GAMEPAD_AXES - 1] = -12000;
	axes[silencer::client_ui::CLIENT_UI_INPUT_MAX_GAMEPAD_AXES] = 12000;

	input.CaptureGamepadBindingEdges(0, axes.data(), static_cast<int>(axes.size()), 1000);
	silencer::ui::UiInputState frame = input.BuildFrame(640, 480, 1.0f, 1.0f / 60.0f);

	CHECK(input.GamepadBindingAxisOverflowCount() == 2);
	REQUIRE(frame.bindingInputs.size() == 2);
	CHECK(frame.bindingInputs[0].kind == silencer::ui::UiBindingInputKind::GamepadAxisMoved);
	CHECK(frame.bindingInputs[0].code == 0);
	CHECK(frame.bindingInputs[0].axisDir == 1);
	CHECK(frame.bindingInputs[1].kind == silencer::ui::UiBindingInputKind::GamepadAxisMoved);
	CHECK(frame.bindingInputs[1].code == silencer::client_ui::CLIENT_UI_INPUT_MAX_GAMEPAD_AXES - 1);
	CHECK(frame.bindingInputs[1].axisDir == -1);
	CHECK(frame.source == silencer::ui::UiFocusSource::Gamepad);
}
