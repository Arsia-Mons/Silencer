#include "doctest.h"

#include "client/ui/ClientUi.h"
#include "ui/runtime/ClayService.h"

#include <sstream>
#include <string>

namespace {

class RecordingClayBackend : public silencer::ui::ClayFrameBackend {
public:
	std::vector<std::string> calls;
	int width = 0;
	int height = 0;
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
	silencer::ui::UiAutomationRegistry registry;
	silencer::ui::UiInputState input;
	input.width = 1280;
	input.height = 720;
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
	CHECK(backend.pointerX == 33.0f);
	CHECK(backend.pointerY == 44.0f);
	CHECK(backend.pointerDown);
	CHECK(backend.scrollY == -2.0f);
	REQUIRE(backend.calls.size() == 6);
	CHECK(backend.calls[0] == "SetCurrentContext");
	CHECK(backend.calls[1] == "SetLayoutDimensions");
	CHECK(backend.calls[2] == "SetPointerState");
	CHECK(backend.calls[3] == "UpdateScrollContainers");
	CHECK(backend.calls[4] == "BeginLayout");
	CHECK(backend.calls[5] == "EndLayout");
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
	CHECK(clientUi.Automation().Elements().empty());
	CHECK(clientUi.DrainActions().empty());
}

TEST_CASE("UiAutomationRegistry supports id and case-insensitive label lookup") {
	silencer::ui::UiAutomationRegistry registry;
	registry.BeginFrame();
	registry.Register(silencer::ui::UiElementMetadata{
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

TEST_CASE("UiAutomationRegistry queues typed actions for interactive widgets") {
	silencer::ui::UiAutomationRegistry registry;
	registry.BeginFrame();

	silencer::ui::UiAutomationWidget widget;
	widget.id = "main_menu.connect";
	widget.labelText = "Connect";
	widget.kind = silencer::ui::UiAutomationWidgetKind::Button;
	widget.uid = 42;
	widget.x = 10;
	widget.y = 20;
	widget.w = 80;
	widget.h = 30;
	registry.RegisterWidget(widget);

	CHECK(registry.InvokeAt(12, 22));

	auto actions = registry.DrainActions();
	REQUIRE(actions.size() == 1);
	CHECK(actions[0].kind == silencer::ui::UiActionKind::Activate);
	CHECK(actions[0].id == "main_menu.connect");
	CHECK(actions[0].value == "Connect");
	CHECK(registry.DrainActions().empty());
}

TEST_CASE("UiAutomationRegistry edits focused text through typed methods") {
	silencer::ui::UiAutomationRegistry registry;
	registry.BeginFrame();

	silencer::ui::UiAutomationWidget widget;
	widget.id = "profile.name";
	widget.labelText = "Name";
	widget.kind = silencer::ui::UiAutomationWidgetKind::TextInput;
	widget.uid = 7;
	widget.value = "ab";
	widget.maxLength = 7;
	registry.RegisterWidget(widget);

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
