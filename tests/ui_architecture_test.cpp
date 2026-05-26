#include "doctest.h"

#include "client/ui/ClientUi.h"
#include "client/ui/ClientUiInput.h"
#include "client/ui/screens/screen.h"
#include "client/ui/screens/screen_context.h"
#include "ui/runtime/ClayService.h"
#include "ui/runtime/UiInputRouter.h"

#include <cstdint>
#include <sstream>
#include <string>

ScreenContext::ScreenContext(Game & game_,
                             World & world_,
                             Renderer & renderer_,
                             Lobby & lobby_,
                             KeyMap & keymap_,
                             Updater & updater_,
                             AmbienceMixer & ambienceMixer_,
                             MapDownloader & mapDownloader_,
                             SDL_Window * & window_,
                             RenderDevice * & renderdevice_)
	: game(game_),
	  world(world_),
	  renderer(renderer_),
	  lobby(lobby_),
	  keymap(keymap_),
	  updater(updater_),
	  ambienceMixer(ambienceMixer_),
	  mapDownloader(mapDownloader_),
	  window(window_),
	  renderdevice(renderdevice_) {}

namespace {

class RecordingClayBackend : public silencer::ui::ClayFrameBackend {
public:
	std::vector<std::string> calls;
	Clay_RenderCommand command{};
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
	Clay_RenderCommandArray EndLayout() override {
		calls.push_back("EndLayout");
		Clay_RenderCommandArray commands{};
		commands.capacity = 1;
		commands.length = 1;
		commands.internalArray = &command;
		return commands;
	}
};

class DestroyCountingScreen final : public Screen {
public:
	explicit DestroyCountingScreen(int * destroyCount)
		: destroyCount_(destroyCount) {}
	~DestroyCountingScreen() override {
		if(destroyCount_) *destroyCount_ += 1;
	}

	void Build(ScreenContext&) override {}
	void Tick(ScreenContext&) override {}
	void Destroy(ScreenContext&) override {}

private:
	int * destroyCount_ = nullptr;
};

template <typename T>
T& UnusedRef() {
	static std::uintptr_t storage = 0;
	return *reinterpret_cast<T*>(&storage);
}

ScreenContext& TestScreenContext() {
	// These tests exercise ClientUi stack/provider mechanics with screens that
	// deliberately ignore ScreenContext. Keeping a real ScreenContext object
	// here avoids pulling the full Game object graph into the unit test target.
	static SDL_Window * window = nullptr;
	static RenderDevice * renderDevice = nullptr;
	static ScreenContext ctx(
		UnusedRef<Game>(),
		UnusedRef<World>(),
		UnusedRef<Renderer>(),
		UnusedRef<Lobby>(),
		UnusedRef<KeyMap>(),
		UnusedRef<Updater>(),
		UnusedRef<AmbienceMixer>(),
		UnusedRef<MapDownloader>(),
		window,
		renderDevice);
	return ctx;
}

Surface& UnusedSurface() {
	return UnusedRef<Surface>();
}

struct HookProbeStats {
	int buildCount = 0;
	int buildUiCount = 0;
	int destroyCount = 0;
	silencer::client_ui::UiScreenEntryId entryId = 0;
	bool sawPush = false;
	bool sawPopCurrent = false;
	bool sawPopTop = false;
	bool sawWriteQueue = false;
};

enum class HookProbeAction {
	None,
	PushAndDeferred,
	PopCurrent,
};

class HookProbeScreen final : public Screen {
public:
	HookProbeScreen(HookProbeStats * stats,
	                bool overlay = false,
	                HookProbeAction action = HookProbeAction::None,
	                HookProbeStats * pushedStats = nullptr,
	                Screen ** pushedScreenOut = nullptr,
	                int * deferredCount = nullptr)
		: stats_(stats),
		  overlay_(overlay),
		  action_(action),
		  pushedStats_(pushedStats),
		  pushedScreenOut_(pushedScreenOut),
		  deferredCount_(deferredCount) {}

	void Build(ScreenContext&) override {
		if(stats_) stats_->buildCount++;
	}

	void Tick(ScreenContext&) override {}

	void BuildUi(ScreenContext&,
	             Surface&,
	             float,
	             silencer::ui::UiInteractionRegistry&) override {
		if(stats_) stats_->buildUiCount++;
		auto navigator = silencer::client_ui::UseScreenNavigator();
		auto queueWrite = silencer::client_ui::UseUiWriteQueue();
		if(stats_){
			stats_->entryId = navigator.currentEntryId;
			stats_->sawPush = static_cast<bool>(navigator.push);
			stats_->sawPopCurrent = static_cast<bool>(navigator.popCurrent);
			stats_->sawPopTop = static_cast<bool>(navigator.popTop);
			stats_->sawWriteQueue = static_cast<bool>(queueWrite);
		}
		if(queued_) return;
		queued_ = true;
		if(action_ == HookProbeAction::PushAndDeferred){
			if(navigator.push && pushedStats_){
				auto pushed = std::make_unique<HookProbeScreen>(pushedStats_);
				if(pushedScreenOut_) *pushedScreenOut_ = pushed.get();
				navigator.push(std::move(pushed));
			}
			if(queueWrite && deferredCount_){
				queueWrite([count = deferredCount_] {
					*count += 1;
				});
			}
		}else if(action_ == HookProbeAction::PopCurrent){
			if(navigator.popCurrent) navigator.popCurrent();
		}
	}

	void Destroy(ScreenContext&) override {
		if(stats_) stats_->destroyCount++;
	}

	bool IsOverlay() const override { return overlay_; }

private:
	HookProbeStats * stats_ = nullptr;
	bool overlay_ = false;
	HookProbeAction action_ = HookProbeAction::None;
	HookProbeStats * pushedStats_ = nullptr;
	Screen ** pushedScreenOut_ = nullptr;
	int * deferredCount_ = nullptr;
	bool queued_ = false;
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

	REQUIRE(commands.length == 1);
	CHECK(commands.internalArray == &backend.command);
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

	REQUIRE(commands.length == 1);
	CHECK(commands.internalArray == &backend.command);
	CHECK(clientUi.Interactions().Elements().empty());
	CHECK(clientUi.DrainActions().empty());
}

TEST_CASE("ClientUi exposes empty provider hooks outside screen declaration") {
	auto navigator = silencer::client_ui::UseScreenNavigator();
	CHECK(navigator.currentEntryId == 0);
	CHECK_FALSE(static_cast<bool>(navigator.push));
	CHECK_FALSE(static_cast<bool>(navigator.popCurrent));
	CHECK_FALSE(static_cast<bool>(navigator.popTop));

	auto queueWrite = silencer::client_ui::UseUiWriteQueue();
	CHECK_FALSE(static_cast<bool>(queueWrite));
}

TEST_CASE("ClientUi clears stale queued writes at the next frame boundary") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	int destroyCount = 0;

	CHECK(clientUi.QueuePushScreen(std::make_unique<DestroyCountingScreen>(&destroyCount)));
	CHECK(clientUi.PendingWriteCount() == 1);
	CHECK(destroyCount == 0);

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	clientUi.BeginFrame(input);
	clientUi.EndFrame();

	CHECK(clientUi.PendingWriteCount() == 0);
	CHECK(destroyCount == 1);
}

TEST_CASE("ClientUi screen hooks queue push and deferred writes until post-render drain") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	HookProbeStats rootStats;
	HookProbeStats pushedStats;
	int deferredCount = 0;
	Screen * pushedScreen = nullptr;

	auto root = std::make_unique<HookProbeScreen>(
		&rootStats,
		false,
		HookProbeAction::PushAndDeferred,
		&pushedStats,
		&pushedScreen,
		&deferredCount);
	Screen * rootScreen = root.get();
	clientUi.PushScreen(std::move(root), TestScreenContext());

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	clientUi.BeginFrame(input);
	clientUi.BuildVisibleScreens(TestScreenContext(), UnusedSurface(), 0.0f);

	CHECK(rootStats.buildCount == 1);
	CHECK(rootStats.buildUiCount == 1);
	CHECK(rootStats.entryId != 0);
	CHECK(rootStats.sawPush);
	CHECK(rootStats.sawPopCurrent);
	CHECK(rootStats.sawPopTop);
	CHECK(rootStats.sawWriteQueue);
	CHECK(clientUi.TopScreen() == rootScreen);
	CHECK(clientUi.PendingWriteCount() == 2);
	CHECK(deferredCount == 0);
	CHECK(pushedStats.buildCount == 0);

	clientUi.EndFrame();
	CHECK(clientUi.TopScreen() == rootScreen);
	CHECK(clientUi.PendingWriteCount() == 2);
	CHECK(deferredCount == 0);

	clientUi.DrainWrites(TestScreenContext());
	CHECK(clientUi.PendingWriteCount() == 0);
	CHECK(deferredCount == 1);
	REQUIRE(pushedScreen != nullptr);
	CHECK(pushedStats.buildCount == 1);
	CHECK(clientUi.TopScreen() == pushedScreen);
}

TEST_CASE("ClientUi popCurrent drains by screen entry id instead of top screen") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	HookProbeStats baseStats;
	HookProbeStats popperStats;
	HookProbeStats topStats;

	auto base = std::make_unique<HookProbeScreen>(&baseStats);
	auto popper = std::make_unique<HookProbeScreen>(
		&popperStats, true, HookProbeAction::PopCurrent);
	auto top = std::make_unique<HookProbeScreen>(&topStats, true);
	Screen * topScreen = top.get();
	clientUi.PushScreen(std::move(base), TestScreenContext());
	clientUi.PushScreen(std::move(popper), TestScreenContext());
	clientUi.PushScreen(std::move(top), TestScreenContext());

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	clientUi.BeginFrame(input);
	clientUi.BuildVisibleScreens(TestScreenContext(), UnusedSurface(), 0.0f);
	clientUi.EndFrame();

	CHECK(baseStats.entryId != 0);
	CHECK(popperStats.entryId != 0);
	CHECK(topStats.entryId != 0);
	CHECK(baseStats.entryId != popperStats.entryId);
	CHECK(popperStats.entryId != topStats.entryId);
	CHECK(clientUi.TopScreen() == topScreen);
	CHECK(clientUi.PendingWriteCount() == 1);

	clientUi.DrainWrites(TestScreenContext());
	CHECK(popperStats.destroyCount == 1);
	CHECK(topStats.destroyCount == 0);
	CHECK(clientUi.TopScreen() == topScreen);
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
