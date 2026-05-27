#include "doctest.h"

#include "client/ui/ClientUi.h"
#include "client/ui/ClientUiInput.h"
#include "client/ui/screens/screen.h"
#include "ui/client_ui_write_drain.h"
#include "ui/game_ui_frame_provider.h"
#include "ui/runtime/ClayService.h"
#include "ui/runtime/UiInputRouter.h"

#include <sstream>
#include <string>

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

struct HookProbeStats {
	int providerCount = 0;
	int destructorCount = 0;
	silencer::client_ui::UiScreenEntryId entryId = 0;
	bool sawPush = false;
	bool sawPopCurrent = false;
	bool sawPopTop = false;
	bool sawWriteQueue = false;
};

struct DeferredActionProbe {
	int providerCount = 0;
	int drainCount = 0;
	std::function<void()> action;
};

enum class HookProbeAction {
	None,
	PushAndDeferred,
	PopCurrent,
};

class HookProbeScreen final : public Screen {
public:
	explicit HookProbeScreen(HookProbeStats * stats, bool overlay = false)
		: stats_(stats), overlay_(overlay) {}
	~HookProbeScreen() override {
		if(stats_) stats_->destructorCount++;
	}

	void Build(ScreenContext&) override {}
	void Tick(ScreenContext&) override {}
	void Destroy(ScreenContext&) override {}
	bool IsOverlay() const override { return overlay_; }

private:
	HookProbeStats * stats_ = nullptr;
	bool overlay_ = false;
};

struct ReplacementLifecycleProbe {
	silencer::client_ui::ScreenStack * stack_ = nullptr;
	Screen * observedTop = nullptr;
	silencer::client_ui::UiScreenEntryId observedBuiltEntryId = 0;
	int buildCount = 0;
	bool attemptBuildMutations = false;
	bool buildPopResult = true;
	bool buildPushResult = true;
	bool buildReplaceResult = true;
	bool attemptDestroyMutations = false;
	bool destroyPopResult = true;
	bool destroyPushResult = true;
	bool destroyReplaceResult = true;
	int destroyCount = 0;
};

struct ReentrantDestructorProbe {
	silencer::client_ui::ScreenStack * stack = nullptr;
	int destructorCount = 0;
	bool popResult = true;
	bool pushResult = true;
	bool replaceResult = true;
};

void AttemptLifecycleMutations(silencer::client_ui::ScreenStack * stack,
                               bool& popResult,
                               bool& pushResult,
                               bool& replaceResult) {
	if(!stack) return;
	popResult = stack->PopForTest();
	pushResult = stack->PushBuiltForTest(std::make_unique<HookProbeScreen>(nullptr));
	replaceResult = stack->ReplaceWithLifecycleForTest(
		std::make_unique<HookProbeScreen>(nullptr), nullptr, nullptr, nullptr);
}

class ReentrantDestructorScreen final : public Screen {
public:
	explicit ReentrantDestructorScreen(ReentrantDestructorProbe * probe)
		: probe_(probe) {}
	~ReentrantDestructorScreen() override {
		if(!probe_) return;
		probe_->destructorCount += 1;
		AttemptLifecycleMutations(probe_->stack,
		                          probe_->popResult,
		                          probe_->pushResult,
		                          probe_->replaceResult);
	}

	void Build(ScreenContext&) override {}
	void Tick(ScreenContext&) override {}
	void Destroy(ScreenContext&) override {}

private:
	ReentrantDestructorProbe * probe_ = nullptr;
};

void ObserveLifecycleBuild(Screen& screen, ScreenContext *, void * userData) {
	auto * probe = static_cast<ReplacementLifecycleProbe *>(userData);
	if(!probe) return;
	probe->buildCount += 1;
	probe->observedTop = probe->stack_ ? probe->stack_->Top() : nullptr;
	probe->observedBuiltEntryId = screen.EntryId();
	if(probe->attemptBuildMutations){
		AttemptLifecycleMutations(probe->stack_,
		                          probe->buildPopResult,
		                          probe->buildPushResult,
		                          probe->buildReplaceResult);
	}
}

void ObserveLifecycleDestroy(Screen&, ScreenContext *, void * userData) {
	auto * probe = static_cast<ReplacementLifecycleProbe *>(userData);
	if(!probe) return;
	probe->destroyCount += 1;
	if(probe->attemptDestroyMutations){
		AttemptLifecycleMutations(probe->stack_,
		                          probe->destroyPopResult,
		                          probe->destroyPushResult,
		                          probe->destroyReplaceResult);
	}
}

void ProbeScreenHooks(HookProbeStats& stats,
                      HookProbeAction action = HookProbeAction::None,
                      HookProbeStats * pushedStats = nullptr,
                      Screen ** pushedScreenOut = nullptr,
                      int * deferredCount = nullptr) {
	stats.providerCount++;
	auto navigator = silencer::client_ui::UseScreenNavigator();
	auto queueWrite = silencer::client_ui::UseUiWriteQueue();
	stats.entryId = navigator.currentEntryId;
	stats.sawPush = static_cast<bool>(navigator.push);
	stats.sawPopCurrent = static_cast<bool>(navigator.popCurrent);
	stats.sawPopTop = static_cast<bool>(navigator.popTop);
	stats.sawWriteQueue = static_cast<bool>(queueWrite);
	if(action == HookProbeAction::PushAndDeferred){
		if(navigator.push && pushedStats){
			auto pushed = std::make_unique<HookProbeScreen>(pushedStats);
			if(pushedScreenOut) *pushedScreenOut = pushed.get();
			navigator.push(std::move(pushed));
		}
		if(queueWrite && deferredCount){
			queueWrite([count = deferredCount] {
				*count += 1;
			});
		}
	}else if(action == HookProbeAction::PopCurrent){
		if(navigator.popCurrent) navigator.popCurrent();
	}
}

void ProbeDeferredActionAfterDeclaration(DeferredActionProbe& probe) {
	probe.providerCount++;
	auto queueWrite = silencer::client_ui::UseUiWriteQueue();
	probe.action = [queueWrite, &probe]() {
		if(queueWrite){
			queueWrite([&probe]() {
				probe.drainCount += 1;
			});
		}
	};
}

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

TEST_CASE("ClientUi dispatch reports feedback without playing game audio") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	input.pointer.x = 12.0f;
	input.pointer.y = 22.0f;
	input.pointer.moved = true;

	clientUi.BeginFrame(input);
	silencer::ui::UiInteractable button;
	button.id = "main_menu.connect";
	button.labelText = "Connect";
	button.kind = silencer::ui::UiInteractableKind::Button;
	button.x = 10;
	button.y = 20;
	button.w = 80;
	button.h = 30;
	REQUIRE(clientUi.Interactions().RegisterInteractable(button));
	clientUi.EndFrame();

	silencer::client_ui::UiDispatchResult first =
		clientUi.DispatchInput(nullptr, input);
	CHECK(first.feedbackRequested);
	REQUIRE(first.unhandledActions.size() == 1);
	CHECK(first.unhandledActions[0].kind == silencer::ui::UiActionKind::Navigate);
	CHECK(first.unhandledActions[0].value == "hover");

	silencer::client_ui::UiDispatchResult second =
		clientUi.DispatchInput(nullptr, input);
	CHECK_FALSE(second.feedbackRequested);
	CHECK(second.unhandledActions.empty());

	input.pointer.x = 200.0f;
	input.pointer.y = 200.0f;
	input.pointer.moved = true;
	silencer::client_ui::UiDispatchResult away =
		clientUi.DispatchInput(nullptr, input);
	CHECK_FALSE(away.feedbackRequested);

	input.pointer.x = 12.0f;
	input.pointer.y = 22.0f;
	input.pointer.moved = true;
	silencer::client_ui::UiDispatchResult returned =
		clientUi.DispatchInput(nullptr, input);
	CHECK(returned.feedbackRequested);
}

TEST_CASE("ClientUi dispatch returns feedback requests for audible actions") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;

	clientUi.BeginFrame(input);
	silencer::ui::UiInteractable button;
	button.id = "main_menu.connect";
	button.labelText = "Connect";
	button.kind = silencer::ui::UiInteractableKind::Button;
	button.x = 10;
	button.y = 20;
	button.w = 80;
	button.h = 30;
	REQUIRE(clientUi.Interactions().RegisterInteractable(button));
	silencer::ui::UiAction activate;
	activate.kind = silencer::ui::UiActionKind::Activate;
	activate.id = "main_menu.connect";
	REQUIRE(clientUi.Interactions().QueueAction(activate));
	clientUi.EndFrame();

	silencer::client_ui::UiDispatchResult result =
		clientUi.DispatchInput(nullptr, input);
	CHECK(result.feedbackRequested);
	REQUIRE(result.unhandledActions.size() == 1);
	CHECK(result.unhandledActions[0].kind == silencer::ui::UiActionKind::Activate);
	CHECK(result.unhandledActions[0].id == "main_menu.connect");
}

TEST_CASE("ClientUi dispatch honors explicit interactable feedback requests") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	input.pointer.x = 12.0f;
	input.pointer.y = 22.0f;
	input.pointer.moved = true;

	clientUi.BeginFrame(input);
	silencer::ui::UiInteractable row;
	row.id = "lobby.game_create.map.2";
	row.labelText = "ctf_compound";
	row.kind = silencer::ui::UiInteractableKind::ListRow;
	row.x = 10;
	row.y = 20;
	row.w = 120;
	row.h = 14;
	row.index = 2;
	row.requestFeedback = true;
	REQUIRE(clientUi.Interactions().RegisterInteractable(row));
	clientUi.EndFrame();

	silencer::client_ui::UiDispatchResult hover =
		clientUi.DispatchInput(nullptr, input);
	CHECK(hover.feedbackRequested);
	REQUIRE(hover.unhandledActions.size() == 1);
	CHECK(hover.unhandledActions[0].kind == silencer::ui::UiActionKind::Navigate);
	CHECK(hover.unhandledActions[0].id == "lobby.game_create.map.2");

	RecordingClayBackend actionBackend;
	silencer::ui::ClayService actionClay(actionBackend);
	silencer::client_ui::ClientUi actionClientUi(actionClay);
	silencer::ui::UiInputState actionInput;
	actionInput.width = 640;
	actionInput.height = 480;

	actionClientUi.BeginFrame(actionInput);
	REQUIRE(actionClientUi.Interactions().RegisterInteractable(row));
	silencer::ui::UiAction select;
	select.kind = silencer::ui::UiActionKind::Select;
	select.id = "lobby.game_create.map.2";
	REQUIRE(actionClientUi.Interactions().QueueAction(select));
	actionClientUi.EndFrame();

	silencer::client_ui::UiDispatchResult selected =
		actionClientUi.DispatchInput(nullptr, actionInput);
	CHECK(selected.feedbackRequested);
	REQUIRE(selected.unhandledActions.size() == 1);
	CHECK(selected.unhandledActions[0].kind == silencer::ui::UiActionKind::Select);
	CHECK(selected.unhandledActions[0].id == "lobby.game_create.map.2");
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

	auto root = std::make_unique<HookProbeScreen>(&rootStats);
	Screen * rootScreen = root.get();
	clientUi.PushBuiltScreenForTest(std::move(root));

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	clientUi.BeginFrame(input);
	clientUi.BuildVisibleScreenProvidersForTest(
		[&](silencer::client_ui::UiScreenEntryId, Screen& screen) {
			CHECK(&screen == rootScreen);
			ProbeScreenHooks(rootStats,
			                 HookProbeAction::PushAndDeferred,
			                 &pushedStats,
			                 &pushedScreen,
			                 &deferredCount);
		});

	CHECK(rootStats.providerCount == 1);
	CHECK(rootStats.entryId != 0);
	CHECK(rootStats.sawPush);
	CHECK(rootStats.sawPopCurrent);
	CHECK(rootStats.sawPopTop);
	CHECK(rootStats.sawWriteQueue);
	CHECK(clientUi.TopScreen() == rootScreen);
	CHECK(clientUi.PendingWriteCount() == 2);
	CHECK(deferredCount == 0);

	clientUi.EndFrame();
	CHECK(clientUi.TopScreen() == rootScreen);
	CHECK(clientUi.PendingWriteCount() == 2);
	CHECK(deferredCount == 0);

	bool sawRenderBeforeDrain = false;
	silencer::client_ui::CompleteRenderedClientUiFrameForTest(clientUi, [&] {
		sawRenderBeforeDrain = true;
		CHECK(clientUi.TopScreen() == rootScreen);
		CHECK(clientUi.PendingWriteCount() == 2);
		CHECK(deferredCount == 0);
	});
	CHECK(sawRenderBeforeDrain);
	CHECK(clientUi.PendingWriteCount() == 0);
	CHECK(deferredCount == 1);
	REQUIRE(pushedScreen != nullptr);
	CHECK(clientUi.TopScreen() == pushedScreen);
}

TEST_CASE("ClientUi hook actions can queue writes after declaration before drain") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	HookProbeStats stats;
	DeferredActionProbe actionProbe;

	auto root = std::make_unique<HookProbeScreen>(&stats);
	Screen * rootScreen = root.get();
	clientUi.PushBuiltScreenForTest(std::move(root));

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	clientUi.BeginFrame(input);
	clientUi.BuildVisibleScreenProvidersForTest(
		[&](silencer::client_ui::UiScreenEntryId, Screen& screen) {
			CHECK(&screen == rootScreen);
			ProbeDeferredActionAfterDeclaration(actionProbe);
		});

	CHECK(actionProbe.providerCount == 1);
	REQUIRE(static_cast<bool>(actionProbe.action));
	CHECK(clientUi.PendingWriteCount() == 0);
	CHECK(actionProbe.drainCount == 0);

	clientUi.EndFrame();
	actionProbe.action();
	CHECK(clientUi.PendingWriteCount() == 1);
	CHECK(actionProbe.drainCount == 0);

	bool sawRenderBeforeDrain = false;
	silencer::client_ui::CompleteRenderedClientUiFrameForTest(clientUi, [&] {
		sawRenderBeforeDrain = true;
		CHECK(clientUi.PendingWriteCount() == 1);
		CHECK(actionProbe.drainCount == 0);
	});
	CHECK(sawRenderBeforeDrain);
	CHECK(clientUi.PendingWriteCount() == 0);
	CHECK(actionProbe.drainCount == 1);
}

TEST_CASE("ClientUi popCurrent drains by screen entry id instead of top screen") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	HookProbeStats baseStats;
	HookProbeStats popperStats;
	HookProbeStats topStats;

	auto base = std::make_unique<HookProbeScreen>(&baseStats);
	Screen * baseScreen = base.get();
	auto popper = std::make_unique<HookProbeScreen>(&popperStats, true);
	Screen * popperScreen = popper.get();
	auto top = std::make_unique<HookProbeScreen>(&topStats, true);
	Screen * topScreen = top.get();
	clientUi.PushBuiltScreenForTest(std::move(base));
	clientUi.PushBuiltScreenForTest(std::move(popper));
	clientUi.PushBuiltScreenForTest(std::move(top));

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	clientUi.BeginFrame(input);
	clientUi.BuildVisibleScreenProvidersForTest(
		[&](silencer::client_ui::UiScreenEntryId, Screen& screen) {
			if(&screen == baseScreen){
				ProbeScreenHooks(baseStats);
			}else if(&screen == popperScreen){
				ProbeScreenHooks(popperStats, HookProbeAction::PopCurrent);
			}else if(&screen == topScreen){
				ProbeScreenHooks(topStats);
			}else{
				FAIL("unexpected visible screen");
			}
		});
	clientUi.EndFrame();

	CHECK(baseStats.entryId != 0);
	CHECK(popperStats.entryId != 0);
	CHECK(topStats.entryId != 0);
	CHECK(baseStats.entryId != popperStats.entryId);
	CHECK(popperStats.entryId != topStats.entryId);
	CHECK(clientUi.TopScreen() == topScreen);
	CHECK(clientUi.PendingWriteCount() == 1);

	clientUi.DrainWritesForTest();
	CHECK(popperStats.destructorCount == 1);
	CHECK(topStats.destructorCount == 0);
	CHECK(clientUi.TopScreen() == topScreen);
}

TEST_CASE("ScreenStack exposes bounded visible screen span") {
	silencer::client_ui::ScreenStack stack;
	for(int i = 0; i < silencer::client_ui::CLIENT_UI_MAX_SCREENS + 3; ++i){
		const bool pushed = stack.PushBuiltForTest(std::make_unique<HookProbeScreen>(nullptr, true));
		CHECK(pushed == (i < silencer::client_ui::CLIENT_UI_MAX_SCREENS));
	}

	CHECK(stack.Size() == silencer::client_ui::CLIENT_UI_MAX_SCREENS);
	CHECK(stack.OverflowCount() == 3);
	silencer::client_ui::VisibleScreenSpan visible = stack.VisibleScreens();
	CHECK(visible.count == silencer::client_ui::CLIENT_UI_MAX_SCREENS);
	for(int i = 0; i < visible.count; ++i){
		REQUIRE(visible[i].screen != nullptr);
		CHECK(visible[i].entryId != 0);
		CHECK(visible[i].entryId == visible[i].screen->EntryId());
		CHECK(visible[i].overlay);
		CHECK(visible[i].visibleIndex == i);
	}
}

TEST_CASE("ClientUi exposes screen stack overflow from drained writes") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);

	for(int i = 0; i < silencer::client_ui::CLIENT_UI_MAX_SCREENS; ++i){
		CHECK(clientUi.PushBuiltScreenForTest(std::make_unique<HookProbeScreen>(nullptr, true)));
	}
	CHECK(clientUi.ScreenStackOverflowCount() == 0);
	CHECK(clientUi.QueuePushScreen(std::make_unique<HookProbeScreen>(nullptr, true)));
	CHECK(clientUi.PendingWriteCount() == 1);

	clientUi.DrainWritesForTest();

	CHECK(clientUi.PendingWriteCount() == 0);
	CHECK(clientUi.ScreenStackOverflowCount() == 1);
}

TEST_CASE("ClientUi exposes dropped write queue overflow") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);

	for(int i = 0; i < silencer::client_ui::CLIENT_UI_MAX_WRITES + 1; ++i){
		const bool queued =
			clientUi.QueuePushScreen(std::make_unique<HookProbeScreen>(nullptr, true));
		CHECK(queued == (i < silencer::client_ui::CLIENT_UI_MAX_WRITES));
	}

	CHECK(clientUi.PendingWriteCount() == silencer::client_ui::CLIENT_UI_MAX_WRITES);
	CHECK(clientUi.WriteOverflowCount() == 1);
}

TEST_CASE("ScreenStack retains pushed screen before lifecycle build") {
	silencer::client_ui::ScreenStack stack;
	ReplacementLifecycleProbe probe;
	probe.stack_ = &stack;
	auto pushed = std::make_unique<HookProbeScreen>(nullptr);
	Screen * pushedScreen = pushed.get();

	CHECK(stack.PushWithLifecycleForTest(std::move(pushed), ObserveLifecycleBuild, &probe));

	CHECK(probe.buildCount == 1);
	CHECK(probe.observedTop == pushedScreen);
	CHECK(probe.observedBuiltEntryId != 0);
	CHECK(stack.Top() == pushedScreen);
	CHECK(stack.TopEntryId() == probe.observedBuiltEntryId);
	CHECK(pushedScreen->EntryId() == probe.observedBuiltEntryId);
}

TEST_CASE("ScreenStack rejects reentrant stack mutations during lifecycle build") {
	silencer::client_ui::ScreenStack stack;
	ReplacementLifecycleProbe probe;
	probe.stack_ = &stack;
	probe.attemptBuildMutations = true;
	auto pushed = std::make_unique<HookProbeScreen>(nullptr);
	Screen * pushedScreen = pushed.get();

	CHECK(stack.PushWithLifecycleForTest(std::move(pushed), ObserveLifecycleBuild, &probe));

	CHECK(probe.buildCount == 1);
	CHECK_FALSE(probe.buildPopResult);
	CHECK_FALSE(probe.buildPushResult);
	CHECK_FALSE(probe.buildReplaceResult);
	CHECK(stack.Size() == 1);
	CHECK(stack.Top() == pushedScreen);
	CHECK(stack.TopEntryId() == pushedScreen->EntryId());
}

TEST_CASE("ScreenStack replacement lifecycle validates before mutation and retains replacement during build") {
	silencer::client_ui::ScreenStack stack;
	HookProbeStats firstStats;
	auto base = std::make_unique<HookProbeScreen>(nullptr);
	auto first = std::make_unique<HookProbeScreen>(&firstStats);
	Screen * firstScreen = first.get();
	ReplacementLifecycleProbe probe;
	probe.stack_ = &stack;
	auto replacement = std::make_unique<HookProbeScreen>(nullptr);
	Screen * replacementScreen = replacement.get();

	CHECK(stack.PushBuiltForTest(std::move(base)));
	CHECK(stack.PushBuiltForTest(std::move(first)));
	CHECK(stack.Top() == firstScreen);
	CHECK_FALSE(stack.ReplaceWithLifecycleForTest(nullptr, ObserveLifecycleBuild, nullptr, &probe));
	CHECK(stack.Top() == firstScreen);
	CHECK(firstStats.destructorCount == 0);

	CHECK(stack.ReplaceWithLifecycleForTest(
		std::move(replacement), ObserveLifecycleBuild, nullptr, &probe));
	CHECK(probe.buildCount == 1);
	CHECK(probe.observedTop == replacementScreen);
	CHECK(probe.observedBuiltEntryId != 0);
	CHECK(stack.Top() == replacementScreen);
	CHECK(stack.TopEntryId() == probe.observedBuiltEntryId);
	CHECK(replacementScreen->EntryId() == probe.observedBuiltEntryId);
	CHECK(firstStats.destructorCount == 1);
}

TEST_CASE("ScreenStack rejects reentrant stack mutations during lifecycle destroy") {
	silencer::client_ui::ScreenStack stack;
	HookProbeStats firstStats;
	auto base = std::make_unique<HookProbeScreen>(nullptr);
	auto first = std::make_unique<HookProbeScreen>(&firstStats);
	Screen * firstScreen = first.get();
	ReplacementLifecycleProbe probe;
	probe.stack_ = &stack;
	probe.attemptDestroyMutations = true;
	auto replacement = std::make_unique<HookProbeScreen>(nullptr);
	Screen * replacementScreen = replacement.get();

	CHECK(stack.PushBuiltForTest(std::move(base)));
	CHECK(stack.PushBuiltForTest(std::move(first)));
	CHECK(stack.Top() == firstScreen);

	CHECK(stack.ReplaceWithLifecycleForTest(
		std::move(replacement), ObserveLifecycleBuild, ObserveLifecycleDestroy, &probe));

	CHECK(probe.destroyCount == 1);
	CHECK_FALSE(probe.destroyPopResult);
	CHECK_FALSE(probe.destroyPushResult);
	CHECK_FALSE(probe.destroyReplaceResult);
	CHECK(probe.buildCount == 1);
	CHECK(stack.Size() == 2);
	CHECK(stack.Top() == replacementScreen);
	CHECK(firstStats.destructorCount == 1);
}

TEST_CASE("ScreenStack rejects destructor-triggered mutations during pop") {
	silencer::client_ui::ScreenStack stack;
	ReentrantDestructorProbe probe;
	probe.stack = &stack;
	auto screen = std::make_unique<ReentrantDestructorScreen>(&probe);
	Screen * screenPtr = screen.get();

	CHECK(stack.PushBuiltForTest(std::move(screen)));
	CHECK(stack.Top() == screenPtr);

	CHECK(stack.PopForTest());

	CHECK(probe.destructorCount == 1);
	CHECK_FALSE(probe.popResult);
	CHECK_FALSE(probe.pushResult);
	CHECK_FALSE(probe.replaceResult);
	CHECK(stack.Size() == 0);
	CHECK(stack.Top() == nullptr);
}

TEST_CASE("ScreenStack rejects destructor-triggered mutations during replace") {
	silencer::client_ui::ScreenStack stack;
	ReentrantDestructorProbe probe;
	probe.stack = &stack;
	auto base = std::make_unique<HookProbeScreen>(nullptr);
	auto first = std::make_unique<ReentrantDestructorScreen>(&probe);
	auto replacement = std::make_unique<HookProbeScreen>(nullptr);
	Screen * replacementScreen = replacement.get();

	CHECK(stack.PushBuiltForTest(std::move(base)));
	CHECK(stack.PushBuiltForTest(std::move(first)));

	CHECK(stack.ReplaceWithLifecycleForTest(
		std::move(replacement), nullptr, nullptr, nullptr));

	CHECK(probe.destructorCount == 1);
	CHECK_FALSE(probe.popResult);
	CHECK_FALSE(probe.pushResult);
	CHECK_FALSE(probe.replaceResult);
	CHECK(stack.Size() == 2);
	CHECK(stack.Top() == replacementScreen);
}

TEST_CASE("ScreenStack rejects destructor-triggered mutations during pop entry") {
	silencer::client_ui::ScreenStack stack;
	ReentrantDestructorProbe probe;
	probe.stack = &stack;
	auto base = std::make_unique<HookProbeScreen>(nullptr);
	auto target = std::make_unique<ReentrantDestructorScreen>(&probe);
	auto top = std::make_unique<HookProbeScreen>(nullptr);
	Screen * topScreen = top.get();

	CHECK(stack.PushBuiltForTest(std::move(base)));
	CHECK(stack.PushBuiltForTest(std::move(target)));
	const auto targetEntryId = stack.TopEntryId();
	CHECK(stack.PushBuiltForTest(std::move(top)));

	CHECK(stack.PopEntryForTest(targetEntryId));

	CHECK(probe.destructorCount == 1);
	CHECK_FALSE(probe.popResult);
	CHECK_FALSE(probe.pushResult);
	CHECK_FALSE(probe.replaceResult);
	CHECK(stack.Size() == 2);
	CHECK(stack.Top() == topScreen);
}

TEST_CASE("ScreenStack rejects destructor-triggered mutations during stack destruction") {
	ReentrantDestructorProbe probe;
	{
		silencer::client_ui::ScreenStack stack;
		probe.stack = &stack;
		CHECK(stack.PushBuiltForTest(std::make_unique<ReentrantDestructorScreen>(&probe)));
		CHECK(stack.Size() == 1);
	}

	CHECK(probe.destructorCount == 1);
	CHECK_FALSE(probe.popResult);
	CHECK_FALSE(probe.pushResult);
	CHECK_FALSE(probe.replaceResult);
}

TEST_CASE("GameUiFrame provider exposes frame data during screen declaration") {
	RecordingClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	HookProbeStats stats;
	auto screen = std::make_unique<HookProbeScreen>(&stats);
	Screen * screenPtr = screen.get();
	clientUi.PushBuiltScreenForTest(std::move(screen));

	silencer::ui::UiInputState input;
	input.width = 320;
	input.height = 200;
	input.uiScale = 2.0f;
	input.pointer.x = 14.0f;
	input.pointer.y = 27.0f;

	CHECK(silencer::game_ui::UseGameUiFrame() == nullptr);
	silencer::game_ui::WithPreparedGameUiFrame(input, 640, 400, [&] {
		clientUi.BuildVisibleScreenProvidersForTest(
			[&](silencer::client_ui::UiScreenEntryId, Screen& screen) {
				REQUIRE(&screen == screenPtr);
				const silencer::game_ui::GameUiFrame * current =
					silencer::game_ui::UseGameUiFrame();
				REQUIRE(current != nullptr);
				CHECK(current->input.width == 320);
				CHECK(current->input.height == 200);
				CHECK(current->input.uiScale == 2.0f);
				CHECK(current->input.pointer.x == 14.0f);
				CHECK(current->input.pointer.y == 27.0f);
				CHECK(current->layout.width == 320.0f);
				CHECK(current->layout.height == 200.0f);
				CHECK(current->pointer.x == 14.0f);
				CHECK(current->pointer.y == 27.0f);
				CHECK(current->surfaceWidth == 640);
				CHECK(current->surfaceHeight == 400);
			});
	});
	CHECK(silencer::game_ui::UseGameUiFrame() == nullptr);
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
	CHECK(frame.source == silencer::ui::UiFocusSource::Keyboard);
	CHECK(frame.pointer.x == 160.0f);
	CHECK(frame.pointer.y == 120.0f);
	CHECK(frame.pointer.down);
	CHECK(frame.pointer.pressed);
	CHECK_FALSE(frame.pointer.moved);
	CHECK(frame.pointer.wheelY == -3.0f);
	CHECK(frame.textInput == "x");
	REQUIRE(frame.navActions.size() == 1);
	CHECK(frame.navActions[0] == silencer::ui::UiNavAction::Confirm);
	REQUIRE(frame.bindingInputs.size() == 1);
	CHECK(frame.bindingInputs[0].code == 44);
	REQUIRE(frame.controlCommands.size() == 1);
	CHECK(frame.controlCommands[0].action.id == "main_menu.connect");

	input.EndFrame();
	input.QueueNavAction(silencer::ui::UiNavAction::Down,
	                     silencer::ui::UiFocusSource::Gamepad);
	frame = input.BuildFrame(640, 480, 2, 1.0f / 60.0f);
	CHECK(frame.source == silencer::ui::UiFocusSource::Gamepad);
	REQUIRE(frame.navActions.size() == 1);
	CHECK(frame.navActions[0] == silencer::ui::UiNavAction::Down);

	input.EndFrame();
	frame = input.BuildFrame(640, 480, 2, 1.0f / 60.0f);
	CHECK(frame.uiScale == 2);
	CHECK(frame.pointer.down);
	CHECK(!frame.pointer.pressed);
	CHECK_FALSE(frame.pointer.moved);
	CHECK(frame.source == silencer::ui::UiFocusSource::Keyboard);
	CHECK(frame.pointer.wheelY == 0.0f);
	CHECK(frame.textInput.empty());
	CHECK(frame.navActions.empty());
	CHECK(frame.bindingInputs.empty());
	CHECK(frame.controlCommands.empty());

	silencer::client_ui::ClientUiInput pointerOnly;
	pointerOnly.QueuePointerSurfaceEvent(10.0f, 20.0f, true, false);
	frame = pointerOnly.BuildFrame(640, 480, 1, 1.0f / 60.0f);
	CHECK(frame.source == silencer::ui::UiFocusSource::Mouse);
	CHECK(frame.pointer.pressed);

	silencer::client_ui::ClientUiInput hoverOnly;
	hoverOnly.QueuePointerSurfaceEvent(14.0f, 28.0f, false, false);
	frame = hoverOnly.BuildFrame(640, 480, 1, 1.0f / 60.0f);
	CHECK(frame.source == silencer::ui::UiFocusSource::Mouse);
	CHECK(frame.pointer.moved);
	CHECK_FALSE(frame.pointer.pressed);
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
	input.pointer.x = 12.0f;
	input.pointer.y = 22.0f;
	input.pointer.pressed = true;

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

TEST_CASE("UiInputRouter leaves focus navigation and activation to the focus runtime") {
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
	second.x = 10;
	second.y = 45;
	second.w = 90;
	second.h = 24;
	registry.RegisterInteractable(second);

	REQUIRE(registry.FocusInteractableById("main_menu.tutorial"));
	const auto * tutorial = registry.FindById("main_menu.tutorial");
	REQUIRE(tutorial != nullptr);
	CHECK(tutorial->focused);

	silencer::ui::UiInputState input;
	input.navActions.push_back(silencer::ui::UiNavAction::Down);
	input.navActions.push_back(silencer::ui::UiNavAction::FocusNext);
	input.navActions.push_back(silencer::ui::UiNavAction::Confirm);

	silencer::ui::UiInputRouter router(registry);
	auto actions = router.Route(input);
	CHECK(actions.empty());

	tutorial = registry.FindById("main_menu.tutorial");
	const auto * options = registry.FindById("main_menu.options");
	REQUIRE(tutorial != nullptr);
	REQUIRE(options != nullptr);
	CHECK(tutorial->focused);
	CHECK_FALSE(options->focused);
}
