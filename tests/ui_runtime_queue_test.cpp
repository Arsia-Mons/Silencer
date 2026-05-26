#include "doctest.h"

#include "runtime/UiActionQueue.h"
#include "runtime/UiInteractionRegistry.h"

#include <string>
#include <utility>
#include <vector>

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

	std::vector<silencer::ui::UiAction> actions = queue.Drain();
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

TEST_CASE("UiInteractionRegistry surfaces action queue overflow diagnostics") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	for(int i = 0; i < silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS + 1; ++i){
		const bool queued = registry.QueueAction(MakeAction(i));
		CHECK(queued == (i < silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS));
	}

	CHECK(registry.PendingActionCount() == silencer::ui::UI_ACTION_QUEUE_MAX_ACTIONS);
	CHECK(registry.ActionOverflowCount() == 1);

	std::vector<silencer::ui::UiAction> actions = registry.DrainActions();
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
