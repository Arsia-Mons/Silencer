#include "doctest.h"

#include "runtime/UiActionQueue.h"
#include "runtime/UiInteractionRegistry.h"

#include <string>
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
