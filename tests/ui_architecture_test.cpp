#include "doctest.h"

#include "client/ui/ClientUi.h"
#include "client/ui/ClientUiInput.h"
#include "ui/runtime/ClayService.h"
#include "ui/runtime/UiInputRouter.h"

#include <sstream>
#include <string>

namespace {

class RecordingClayBackend : public silencer::ui::ClayFrameBackend {
public:
	std::vector<std::string> calls;
	int width = 0;
	int height = 0;
	float uiScale = 0.0f;
	float pointerX = 0.0f;
	float pointerY = 0.0f;
	bool pointerDown = false;
	float scrollY = 0.0f;

	void SetCurrentContext() override { calls.push_back("SetCurrentContext"); }
	void SetLayoutDimensions(int w, int h) override {
		width = w;
		height = h;
		calls.push_back("SetLayoutDimensions");
	}
	void SetUiScale(float scale) override {
		uiScale = scale;
		calls.push_back("SetUiScale");
	}
	void SetPointerState(float x, float y, bool down) override {
		pointerX = x;
		pointerY = y;
		pointerDown = down;
		calls.push_back("SetPointerState");
	}
	void UpdateScrollContainers(float, float y, float) override {
		scrollY = y;
		calls.push_back("UpdateScrollContainers");
	}
	void BeginLayout() override { calls.push_back("BeginLayout"); }
	std::vector<silencer::ui::UiRenderCommand> EndLayout() override {
		calls.push_back("EndLayout");
		return { silencer::ui::UiRenderCommand{ "frame" } };
	}
};

}  // namespace

TEST_CASE("ClayService uses the required central frame lifecycle order") {
	RecordingClayBackend backend;
	silencer::ui::ClayService service(backend);
	silencer::ui::UiInteractionRegistry registry;
	silencer::ui::UiInputState input;
	input.width = 1280;
	input.height = 720;
	input.uiScale = 2;
	input.deltaTimeSeconds = 1.0f / 60.0f;
	input.pointer.x = 33.0f;
	input.pointer.y = 44.0f;
	input.pointer.down = true;
	input.pointer.wheelY = -2.0f;

	service.BeginFrame(input, registry);
	auto commands = service.EndFrame();

	REQUIRE(commands.size() == 1);
	CHECK(backend.width == 1280);
	CHECK(backend.height == 720);
	CHECK(backend.uiScale == 2);
	CHECK(backend.pointerX == 33.0f);
	CHECK(backend.pointerY == 44.0f);
	CHECK(backend.pointerDown);
	CHECK(backend.scrollY == -2.0f);
	REQUIRE(backend.calls.size() == 7);
	CHECK(backend.calls[0] == "SetCurrentContext");
	CHECK(backend.calls[1] == "SetLayoutDimensions");
	CHECK(backend.calls[2] == "SetUiScale");
	CHECK(backend.calls[3] == "SetPointerState");
	CHECK(backend.calls[4] == "UpdateScrollContainers");
	CHECK(backend.calls[5] == "BeginLayout");
	CHECK(backend.calls[6] == "EndLayout");
}

TEST_CASE("ClientUi owns the frame lifecycle without demo-screen metadata") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;

	clientUi.BeginFrame(input);
	auto commands = clientUi.EndFrame();

	REQUIRE(commands.size() == 1);
	CHECK(clientUi.Interactions().Elements().empty());
	CHECK(clientUi.DrainActions().empty());
}

TEST_CASE("UiInteractionRegistry supports id and case-insensitive label lookup") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();
	registry.Register(silencer::ui::UiElementSnapshot{
		"main_menu.connect",
		silencer::ui::UiElementKind::Button,
		"Connect",
		"",
		silencer::ui::UiRect{ 1.0f, 2.0f, 3.0f, 4.0f },
		true,
		false,
		false,
	});

	CHECK(registry.FindById("main_menu.connect") != nullptr);
	CHECK(registry.FindByLabel("connect") != nullptr);
	CHECK(registry.FindByLabel("missing") == nullptr);
}

TEST_CASE("UiInteractionRegistry queues typed actions for interactive widgets") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	silencer::ui::UiInteractable widget;
	widget.id = "main_menu.connect";
	widget.labelText = "Connect";
	widget.kind = silencer::ui::UiInteractableKind::Button;
	widget.uid = 42;
	widget.x = 10;
	widget.y = 20;
	widget.w = 80;
	widget.h = 30;
	registry.RegisterInteractable(widget);

	CHECK(registry.PressAt(12, 22));

	auto actions = registry.DrainActions();
	REQUIRE(actions.size() == 1);
	CHECK(actions[0].kind == silencer::ui::UiActionKind::Activate);
	CHECK(actions[0].id == "main_menu.connect");
	CHECK(actions[0].value == "Connect");
	CHECK(registry.DrainActions().empty());
}

TEST_CASE("UiInteractionRegistry edits focused text through typed methods") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	silencer::ui::UiInteractable widget;
	widget.id = "profile.name";
	widget.labelText = "Name";
	widget.kind = silencer::ui::UiInteractableKind::TextInput;
	widget.uid = 7;
	widget.value = "ab";
	widget.maxLength = 7;
	registry.RegisterInteractable(widget);

	REQUIRE(registry.FocusTextInputByUid(7));
	(void)registry.DrainActions();
	CHECK(registry.DispatchTextInput('c'));
	auto actions = registry.DrainActions();
	REQUIRE(actions.size() == 1);
	CHECK(actions[0].kind == silencer::ui::UiActionKind::SetText);
	CHECK(actions[0].id == "profile.name");
	CHECK(actions[0].value == "abc");

	CHECK(registry.BackspaceFocusedText());
	actions = registry.DrainActions();
	REQUIRE(actions.size() == 1);
	CHECK(actions[0].kind == silencer::ui::UiActionKind::SetText);
	CHECK(actions[0].value == "ab");

	CHECK(registry.SubmitFocusedText());
	actions = registry.DrainActions();
	REQUIRE(actions.size() == 1);
	CHECK(actions[0].kind == silencer::ui::UiActionKind::SubmitText);
	CHECK(actions[0].id == "profile.name");
	CHECK(actions[0].value == "ab");
}

TEST_CASE("UiInteractionRegistry reports retained text focus during frame declaration") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	silencer::ui::UiInteractable widget;
	widget.id = "profile.name";
	widget.labelText = "Name";
	widget.kind = silencer::ui::UiInteractableKind::TextInput;
	widget.uid = 7;
	widget.value = "ab";
	registry.RegisterInteractable(widget);

	REQUIRE(registry.FocusTextInputByUid(7));
	(void)registry.DrainActions();

	registry.BeginFrame();
	CHECK(registry.IsTextInputFocused(7));
	CHECK_FALSE(registry.IsTextInputFocused(8));

	registry.RegisterInteractable(widget);
	CHECK(registry.IsTextInputFocused(7));
}

TEST_CASE("UiInteractionRegistry keeps active text focus through hover") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	silencer::ui::UiInteractable input;
	input.id = "profile.name";
	input.labelText = "Name";
	input.kind = silencer::ui::UiInteractableKind::TextInput;
	input.uid = 7;
	input.value = "ab";
	input.x = 10;
	input.y = 10;
	input.w = 120;
	input.h = 24;
	registry.RegisterInteractable(input);

	silencer::ui::UiInteractable button;
	button.id = "profile.save";
	button.labelText = "Save";
	button.kind = silencer::ui::UiInteractableKind::Button;
	button.x = 10;
	button.y = 50;
	button.w = 120;
	button.h = 24;
	registry.RegisterInteractable(button);

	REQUIRE(registry.FocusTextInputByUid(7));
	(void)registry.DrainActions();

	CHECK_FALSE(registry.FocusHovered(20.0f, 60.0f));
	CHECK(registry.IsTextInputFocused(7));
	CHECK(registry.DrainActions().empty());

	CHECK(registry.PressAt(20, 60));
	CHECK_FALSE(registry.IsTextInputFocused(7));
	auto actions = registry.DrainActions();
	REQUIRE(actions.size() == 1);
	CHECK(actions[0].kind == silencer::ui::UiActionKind::Activate);
	CHECK(actions[0].id == "profile.save");
	CHECK(actions[0].value == "Save");
}

TEST_CASE("UiInteractionRegistry still moves non-text focus on hover") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	silencer::ui::UiInteractable first;
	first.id = "main_menu.tutorial";
	first.labelText = "Tutorial";
	first.kind = silencer::ui::UiInteractableKind::Button;
	first.x = 10;
	first.y = 10;
	first.w = 90;
	first.h = 24;
	registry.RegisterInteractable(first);

	silencer::ui::UiInteractable second;
	second.id = "main_menu.options";
	second.labelText = "Options";
	second.kind = silencer::ui::UiInteractableKind::Button;
	second.x = 120;
	second.y = 10;
	second.w = 90;
	second.h = 24;
	registry.RegisterInteractable(second);

	REQUIRE(registry.FocusInteractableById("main_menu.tutorial"));
	const auto * tutorial = registry.FindById("main_menu.tutorial");
	REQUIRE(tutorial != nullptr);
	CHECK(tutorial->focused);

	CHECK(registry.FocusHovered(130.0f, 20.0f));
	tutorial = registry.FindById("main_menu.tutorial");
	const auto * options = registry.FindById("main_menu.options");
	REQUIRE(tutorial != nullptr);
	REQUIRE(options != nullptr);
	CHECK_FALSE(tutorial->focused);
	CHECK(options->focused);
	auto actions = registry.DrainActions();
	REQUIRE(actions.size() == 1);
	CHECK(actions[0].kind == silencer::ui::UiActionKind::Navigate);
	CHECK(actions[0].id == "main_menu.options");
	CHECK(actions[0].value == "hover");

	CHECK(registry.FocusHovered(300.0f, 300.0f));
	options = registry.FindById("main_menu.options");
	REQUIRE(options != nullptr);
	CHECK_FALSE(options->focused);
	CHECK(registry.DrainActions().empty());

	REQUIRE(registry.FocusInteractableById("main_menu.tutorial"));
	tutorial = registry.FindById("main_menu.tutorial");
	REQUIRE(tutorial != nullptr);
	CHECK(tutorial->focused);

	CHECK(registry.FocusHovered(20.0f, 20.0f));
	CHECK(registry.DrainActions().empty());

	CHECK(registry.FocusHovered(310.0f, 310.0f));
	tutorial = registry.FindById("main_menu.tutorial");
	REQUIRE(tutorial != nullptr);
	CHECK_FALSE(tutorial->focused);
	CHECK(registry.DrainActions().empty());

	REQUIRE(registry.FocusInteractableById("main_menu.tutorial"));
	CHECK_FALSE(registry.FocusHovered(320.0f, 320.0f));
	tutorial = registry.FindById("main_menu.tutorial");
	REQUIRE(tutorial != nullptr);
	CHECK(tutorial->focused);
	CHECK(registry.DrainActions().empty());
}

TEST_CASE("ClientUiInput normalizes pointer and drains frame-local inputs") {
	silencer::client_ui::ClientUiInput input;
	input.QueuePointerWindowEvent(50.0f, 25.0f, 200, 100, 640, 480, true, false);
	input.AddWheelDelta(0.0f, -3.0f);
	input.QueueTextInput('x');
	input.QueueNavAction(silencer::ui::UiNavAction::Confirm);
	input.QueueBindingKeyDown(44);

	silencer::ui::UiAction action;
	action.kind = silencer::ui::UiActionKind::Activate;
	action.id = "main_menu.connect";
	input.QueueControlAction(action);

	auto frame = input.BuildFrame(640, 480, 1, 0.0f);
	CHECK(frame.uiScale == 1);
	CHECK(frame.pointer.x == 160.0f);
	CHECK(frame.pointer.y == 120.0f);
	CHECK(frame.pointer.down);
	CHECK(frame.pointer.pressed);
	CHECK(frame.pointer.wheelY == -3.0f);
	CHECK(frame.textInput == "x");
	REQUIRE(frame.navActions.size() == 1);
	CHECK(frame.navActions[0] == silencer::ui::UiNavAction::Confirm);
	REQUIRE(frame.bindingInputs.size() == 1);
	CHECK(frame.bindingInputs[0].code == 44);
	REQUIRE(frame.controlCommands.size() == 1);
	CHECK(frame.controlCommands[0].action.id == "main_menu.connect");

	input.EndFrame();
	frame = input.BuildFrame(640, 480, 2, 1.0f / 60.0f);
	CHECK(frame.uiScale == 2);
	CHECK(frame.pointer.down);
	CHECK(!frame.pointer.pressed);
	CHECK(frame.pointer.wheelY == 0.0f);
	CHECK(frame.textInput.empty());
	CHECK(frame.navActions.empty());
	CHECK(frame.bindingInputs.empty());
	CHECK(frame.controlCommands.empty());
}

TEST_CASE("UiInputRouter routes control commands and binding capture through typed actions") {
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();

	silencer::ui::UiInteractable widget;
	widget.id = "main_menu.connect";
	widget.labelText = "Connect";
	widget.kind = silencer::ui::UiInteractableKind::Button;
	widget.x = 10;
	widget.y = 20;
	widget.w = 80;
	widget.h = 30;
	registry.RegisterInteractable(widget);

	silencer::ui::UiInputState input;
	silencer::ui::UiControlCommand click;
	click.kind = silencer::ui::UiControlCommandKind::PointerPress;
	click.x = 12;
	click.y = 22;
	input.controlCommands.push_back(click);

	silencer::ui::UiBindingInput binding;
	binding.kind = silencer::ui::UiBindingInputKind::KeyboardKeyDown;
	binding.code = 44;
	input.bindingInputs.push_back(binding);

	silencer::ui::UiInputRouter router(registry);
	auto actions = router.Route(input);
	REQUIRE(actions.size() == 2);
	CHECK(actions[0].kind == silencer::ui::UiActionKind::Activate);
	CHECK(actions[0].id == "main_menu.connect");
	CHECK(actions[1].kind == silencer::ui::UiActionKind::CaptureBinding);
	CHECK(actions[1].binding.code == 44);
}
