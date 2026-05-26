#include "doctest.h"

#include "client/ui/ClientUi.h"
#include "client/ui/screens/screen.h"
#include "ui/focus/UiFocus.h"
#include "ui/primitives/focusable.h"
#include "ui/runtime/ClayService.h"

#include "clay/clay.h"

#include <cstdlib>
#include <cstring>
#include <memory>

namespace {

Clay_Context* g_clay = nullptr;
void* g_clayMemory = nullptr;

void OnClayError(Clay_ErrorData error) {
	(void)error;
}

Clay_Dimensions MeasureText(Clay_StringSlice text,
                            Clay_TextElementConfig*,
                            void*) {
	return Clay_Dimensions{static_cast<float>(text.length) * 8.0f, 16.0f};
}

void EnsureClay() {
	if(g_clay){
		Clay_SetCurrentContext(g_clay);
		return;
	}

	uint32_t clayMemorySize = Clay_MinMemorySize();
	g_clayMemory = std::malloc(clayMemorySize);
	REQUIRE(g_clayMemory != nullptr);

	Clay_Arena arena =
		Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, g_clayMemory);
	g_clay = Clay_Initialize(arena,
	                         Clay_Dimensions{640.0f, 480.0f},
	                         Clay_ErrorHandler{OnClayError, nullptr});
	REQUIRE(g_clay != nullptr);
	Clay_SetMeasureTextFunction(MeasureText, nullptr);
}

Clay_ElementId TestId(const char* name) {
	return Clay_GetElementId(Clay_String{
		false,
		static_cast<int32_t>(std::strlen(name)),
		name,
	});
}

bool SameId(Clay_ElementId a, Clay_ElementId b) {
	return a.id != 0 && a.id == b.id;
}

void FocusBox(Clay_ElementId id,
              bool disabled = false,
              silencer::ui::UiNavRules nav = {},
              std::function<void()> onConfirm = {},
              std::function<void()> onFocus = {}) {
	silencer::ui::primitives::Focusable({
		id,
		disabled,
		nav,
		onConfirm,
		onFocus,
	}, [](const silencer::ui::UiFocusableState&) {});
	CLAY({
		.id = id,
		.layout = {
			.sizing = { CLAY_SIZING_FIXED(60), CLAY_SIZING_FIXED(32) },
		},
	}) {}
}

void RuntimeButton(silencer::ui::UiInteractionRegistry& interactions,
                   Clay_ElementId id,
                   const char * actionId) {
	CLAY({
		.id = id,
		.layout = {
			.sizing = { CLAY_SIZING_FIXED(80), CLAY_SIZING_FIXED(32) },
		},
	}) {
		silencer::ui::UiInteractable widget;
		widget.id = actionId;
		widget.labelText = actionId;
		widget.kind = silencer::ui::UiInteractableKind::Button;
		widget.clayId = id;
		widget.hasClayId = true;
		interactions.RegisterInteractable(widget);
	}
}

template <typename Build>
void RunFocusFrame(silencer::ui::UiFocusRuntime& focus,
                   const silencer::ui::UiFocusInputFrame& input,
                   Build build,
                   Clay_Dimensions dimensions = {640.0f, 480.0f},
                   Clay_Vector2 pointer = {-1000.0f, -1000.0f}) {
	EnsureClay();
	silencer::ui::UiFocusInputFrame frameInput = input;
	frameInput.pointerX = pointer.x;
	frameInput.pointerY = pointer.y;
	silencer::ui::ui_focus_set_current(&focus);
	Clay_SetLayoutDimensions(dimensions);
	Clay_SetPointerState(pointer, frameInput.pointerDown);
	silencer::ui::ui_focus_begin_frame(frameInput);
	Clay_BeginLayout();
	CLAY({
		.id = TestId("FocusTestRoot"),
		.layout = {
			.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.childGap = 8,
		},
	}) {
		build();
	}
	(void)Clay_EndLayout();
	silencer::ui::ui_focus_end_layout(frameInput);
}

void SimpleStackScope(Clay_ElementId scope,
                      Clay_ElementId a,
                      Clay_ElementId b,
                      Clay_ElementId c,
                      bool bDisabled = false) {
	silencer::ui::ui_focus_push_scope({scope});
	CLAY({
		.id = TestId("FocusStack"),
		.layout = {
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.childGap = 8,
		},
	}) {
		FocusBox(a);
		FocusBox(b, bDisabled);
		FocusBox(c);
	}
	silencer::ui::ui_focus_pop_scope();
}

class RealClayBackend : public silencer::ui::ClayFrameBackend {
public:
	void SetCurrentContext() override { EnsureClay(); }
	void SetLayoutDimensions(int width, int height) override {
		Clay_SetLayoutDimensions(Clay_Dimensions{
			static_cast<float>(width), static_cast<float>(height)});
	}
	void SetUiScale(float) override {}
	void SetPointerState(float x, float y, bool down) override {
		Clay_SetPointerState(Clay_Vector2{x, y}, down);
	}
	void UpdateScrollContainers(float wheelX,
	                            float wheelY,
	                            float deltaTimeSeconds) override {
		Clay_UpdateScrollContainers(true,
		                            Clay_Vector2{wheelX, wheelY},
		                            deltaTimeSeconds);
	}
	void BeginLayout() override { Clay_BeginLayout(); }
	Clay_RenderCommandArray EndLayout() override { return Clay_EndLayout(); }
};

class FrameProbeScreen final : public Screen {
public:
	explicit FrameProbeScreen(bool overlay)
		: overlay_(overlay) {}

	void Build(ScreenContext&) override {}
	void Tick(ScreenContext&) override {}
	void Destroy(ScreenContext&) override {}
	bool IsOverlay() const override { return overlay_; }

private:
	bool overlay_ = false;
};

}  // namespace

TEST_CASE("UiFocus navigates from harvested Clay rectangles") {
	silencer::ui::UiFocusRuntime focus;
	silencer::ui::ui_focus_init(&focus);

	Clay_ElementId scope = TestId("StackScope");
	Clay_ElementId a = TestId("StackA");
	Clay_ElementId b = TestId("StackB");
	Clay_ElementId c = TestId("StackC");

	RunFocusFrame(focus, {}, [&] { SimpleStackScope(scope, a, b, c); });
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), a));

	silencer::ui::UiFocusInputFrame down;
	down.navDown = true;
	down.source = silencer::ui::UiFocusSource::Keyboard;
	RunFocusFrame(focus, down, [&] { SimpleStackScope(scope, a, b, c); });
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), b));

	down.source = silencer::ui::UiFocusSource::Gamepad;
	RunFocusFrame(focus, down, [&] { SimpleStackScope(scope, a, b, c); });
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), c));
	CHECK(silencer::ui::ui_focus_source_for_scope(scope) ==
	      silencer::ui::UiFocusSource::Gamepad);
}

TEST_CASE("UiFocus skips disabled controls and honors local nav rules") {
	silencer::ui::UiFocusRuntime focus;
	silencer::ui::ui_focus_init(&focus);

	Clay_ElementId scope = TestId("RulesScope");
	Clay_ElementId a = TestId("RulesA");
	Clay_ElementId b = TestId("RulesB");
	Clay_ElementId c = TestId("RulesC");

	silencer::ui::UiNavRules aRules;
	aRules.left.kind = silencer::ui::UiNavRuleKind::Stop;
	aRules.right.kind = silencer::ui::UiNavRuleKind::Explicit;
	aRules.right.explicitTarget = c;
	silencer::ui::UiNavRules cRules;
	cRules.right.kind = silencer::ui::UiNavRuleKind::Wrap;

	auto build = [&] {
		silencer::ui::ui_focus_push_scope({scope});
		CLAY({
			.id = TestId("RulesRow"),
			.layout = {
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
				.childGap = 8,
			},
		}) {
			FocusBox(a, false, aRules);
			FocusBox(b, true);
			FocusBox(c, false, cRules);
		}
		silencer::ui::ui_focus_pop_scope();
	};

	RunFocusFrame(focus, {}, build);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), a));

	silencer::ui::UiFocusInputFrame left;
	left.navLeft = true;
	RunFocusFrame(focus, left, build);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), a));

	silencer::ui::UiFocusInputFrame right;
	right.navRight = true;
	RunFocusFrame(focus, right, build);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), c));

	RunFocusFrame(focus, right, build);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), a));
}

TEST_CASE("UiFocus modal scopes trap navigation and parent focus resumes") {
	silencer::ui::UiFocusRuntime focus;
	silencer::ui::ui_focus_init(&focus);

	Clay_ElementId parent = TestId("ParentScope");
	Clay_ElementId modal = TestId("ModalScope");
	Clay_ElementId a = TestId("ParentA");
	Clay_ElementId b = TestId("ParentB");
	Clay_ElementId m1 = TestId("ModalOne");
	Clay_ElementId m2 = TestId("ModalTwo");

	auto build = [&](bool showModal) {
		silencer::ui::ui_focus_push_scope({parent});
		FocusBox(a);
		FocusBox(b);
		silencer::ui::ui_focus_pop_scope();
		if(showModal){
			silencer::ui::ui_focus_push_scope({modal, true, true});
			FocusBox(m1);
			FocusBox(m2);
			silencer::ui::ui_focus_pop_scope();
		}
	};

	RunFocusFrame(focus, {}, [&] { build(false); });
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(parent), a));

	silencer::ui::UiFocusInputFrame down;
	down.navDown = true;
	RunFocusFrame(focus, down, [&] { build(false); });
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(parent), b));

	RunFocusFrame(focus, {}, [&] { build(true); });
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(modal), m1));
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(parent), b));

	RunFocusFrame(focus, down, [&] { build(true); });
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(modal), m2));
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(parent), b));
}

TEST_CASE("UiFocus pointer release confirms only the original hovered target") {
	silencer::ui::UiFocusRuntime focus;
	silencer::ui::ui_focus_init(&focus);

	Clay_ElementId scope = TestId("PointerScope");
	Clay_ElementId a = TestId("PointerA");
	Clay_ElementId b = TestId("PointerB");
	int confirmCount = 0;

	auto build = [&] {
		silencer::ui::ui_focus_push_scope({scope});
		FocusBox(a, false, {}, [&] { confirmCount++; });
		FocusBox(b, false, {}, [&] { confirmCount++; });
		silencer::ui::ui_focus_pop_scope();
	};

	RunFocusFrame(focus, {}, build);
	Clay_ElementData aData = Clay_GetElementData(a);
	REQUIRE(aData.found);
	Clay_Vector2 insideA = {
		aData.boundingBox.x + aData.boundingBox.width * 0.5f,
		aData.boundingBox.y + aData.boundingBox.height * 0.5f,
	};
	Clay_Vector2 outside = {
		aData.boundingBox.x + aData.boundingBox.width + 140.0f,
		aData.boundingBox.y + aData.boundingBox.height + 140.0f,
	};

	silencer::ui::UiFocusInputFrame press;
	press.pointerPressed = true;
	press.pointerDown = true;
	press.source = silencer::ui::UiFocusSource::Mouse;
	RunFocusFrame(focus, press, build, {640.0f, 480.0f}, insideA);
	CHECK(confirmCount == 0);

	silencer::ui::UiFocusInputFrame hold;
	hold.pointerDown = true;
	hold.source = silencer::ui::UiFocusSource::Mouse;
	RunFocusFrame(focus, hold, build, {640.0f, 480.0f}, outside);

	silencer::ui::UiFocusInputFrame release;
	release.pointerReleased = true;
	release.source = silencer::ui::UiFocusSource::Mouse;
	RunFocusFrame(focus, release, build, {640.0f, 480.0f}, outside);
	CHECK(confirmCount == 0);

	RunFocusFrame(focus, press, build, {640.0f, 480.0f}, insideA);
	RunFocusFrame(focus, hold, build, {640.0f, 480.0f}, insideA);
	RunFocusFrame(focus, release, build, {640.0f, 480.0f}, insideA);
	CHECK(confirmCount == 1);
}

TEST_CASE("UiFocus pointer movement follows hover through harvested layout") {
	silencer::ui::UiFocusRuntime focus;
	silencer::ui::ui_focus_init(&focus);

	Clay_ElementId scope = TestId("HoverScope");
	Clay_ElementId a = TestId("HoverA");
	Clay_ElementId b = TestId("HoverB");

	auto build = [&] {
		silencer::ui::ui_focus_push_scope({scope});
		FocusBox(a);
		FocusBox(b);
		silencer::ui::ui_focus_pop_scope();
	};

	RunFocusFrame(focus, {}, build);
	Clay_ElementData bData = Clay_GetElementData(b);
	REQUIRE(bData.found);

	silencer::ui::UiFocusInputFrame moved;
	moved.pointerMoved = true;
	moved.source = silencer::ui::UiFocusSource::Mouse;
	Clay_Vector2 insideB = {
		bData.boundingBox.x + bData.boundingBox.width * 0.5f,
		bData.boundingBox.y + bData.boundingBox.height * 0.5f,
	};
	RunFocusFrame(focus, moved, build, {640.0f, 480.0f}, insideB);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), b));
	CHECK(silencer::ui::ui_focus_source_for_scope(scope) ==
	      silencer::ui::UiFocusSource::Mouse);

	Clay_Vector2 outside = {
		bData.boundingBox.x + bData.boundingBox.width + 200.0f,
		bData.boundingBox.y + bData.boundingBox.height + 200.0f,
	};
	RunFocusFrame(focus, moved, build, {640.0f, 480.0f}, outside);
	CHECK(silencer::ui::ui_focus_focused_id_for_scope(scope).id == 0);

	RunFocusFrame(focus, {}, build);
	CHECK(silencer::ui::ui_focus_focused_id_for_scope(scope).id == 0);

	silencer::ui::UiFocusInputFrame down;
	down.navDown = true;
	down.source = silencer::ui::UiFocusSource::Keyboard;
	RunFocusFrame(focus, down, build);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), a));
}

TEST_CASE("UiFocus failed scope pushes unwind without corrupting parent scope") {
	silencer::ui::UiFocusRuntime focus;
	silencer::ui::ui_focus_init(&focus, {
		.maxFocusScopes = 1,
		.maxFocusablesPerScope = 8,
	});

	Clay_ElementId parent = TestId("OverflowParentScope");
	Clay_ElementId overflow = TestId("OverflowNoopScope");
	Clay_ElementId parentFirst = TestId("OverflowParentFirst");
	Clay_ElementId parentSecond = TestId("OverflowParentSecond");
	Clay_ElementId leakedItem = TestId("OverflowLeakedItem");

	RunFocusFrame(focus, {}, [&] {
		silencer::ui::ui_focus_push_scope({parent});
		FocusBox(parentFirst);
		silencer::ui::ui_focus_push_scope({overflow});
		FocusBox(leakedItem);
		silencer::ui::ui_focus_pop_scope();
		FocusBox(parentSecond);
		silencer::ui::ui_focus_pop_scope();
	});

	CHECK(silencer::ui::ui_focus_error_count() == 1);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(parent), parentFirst));

	silencer::ui::UiFocusInputFrame down;
	down.navDown = true;
	RunFocusFrame(focus, down, [&] {
		silencer::ui::ui_focus_push_scope({parent});
		FocusBox(parentFirst);
		FocusBox(parentSecond);
		silencer::ui::ui_focus_pop_scope();
	});
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(parent), parentSecond));
}

TEST_CASE("UiFocus unique-scope creation overflow does not leak declarations") {
	silencer::ui::UiFocusRuntime focus;
	silencer::ui::ui_focus_init(&focus, {
		.maxFocusScopes = 1,
		.maxFocusablesPerScope = 8,
	});

	Clay_ElementId firstScope = TestId("CreationOverflowFirstScope");
	Clay_ElementId secondScope = TestId("CreationOverflowSecondScope");
	Clay_ElementId firstItem = TestId("CreationOverflowFirstItem");
	Clay_ElementId leakedItem = TestId("CreationOverflowLeakedItem");

	RunFocusFrame(focus, {}, [&] {
		silencer::ui::ui_focus_push_scope({firstScope});
		FocusBox(firstItem);
		silencer::ui::ui_focus_pop_scope();
	});
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(firstScope), firstItem));

	RunFocusFrame(focus, {}, [&] {
		silencer::ui::ui_focus_push_scope({secondScope});
		FocusBox(leakedItem);
		silencer::ui::ui_focus_pop_scope();
	});
	CHECK(silencer::ui::ui_focus_error_count() == 1);
	CHECK(silencer::ui::ui_focus_focused_id_for_scope(secondScope).id == 0);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(firstScope), firstItem));
}

TEST_CASE("ClientUi owns the UiFocus frame lifecycle") {
	RealClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	Clay_ElementId scope = TestId("ClientUiFocusScope");
	Clay_ElementId first = TestId("ClientUiFocusFirst");
	Clay_ElementId second = TestId("ClientUiFocusSecond");

	clientUi.BeginFrame(input);
	CLAY({
		.id = TestId("ClientUiFocusStack"),
		.layout = {
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.childGap = 8,
		},
	}) {
		silencer::ui::ui_focus_push_scope({scope});
		FocusBox(first);
		FocusBox(second);
		silencer::ui::ui_focus_pop_scope();
	}
	clientUi.EndFrame();
	CHECK(SameId(
		silencer::ui::ui_focus_focused_id_for_scope(scope),
		first));

	input.navActions.push_back(silencer::ui::UiNavAction::Down);
	input.source = silencer::ui::UiFocusSource::Gamepad;
	input.pointer.down = true;
	clientUi.BeginFrame(input);
	CLAY({
		.id = TestId("ClientUiFocusStack"),
		.layout = {
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.childGap = 8,
		},
	}) {
		silencer::ui::ui_focus_push_scope({scope});
		FocusBox(first);
		FocusBox(second);
		silencer::ui::ui_focus_pop_scope();
	}
	clientUi.EndFrame();
	CHECK(SameId(
		silencer::ui::ui_focus_focused_id_for_scope(scope),
		second));
	CHECK(silencer::ui::ui_focus_source_for_scope(scope) ==
	      silencer::ui::UiFocusSource::Gamepad);
}

TEST_CASE("ClientUi wraps overlay screens in root-attached frame geometry") {
	RealClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);
	int baseBuilds = 0;
	int overlayBuilds = 0;
	silencer::client_ui::UiScreenEntryId baseEntry = 0;
	silencer::client_ui::UiScreenEntryId overlayEntry = 0;

	auto base = std::make_unique<FrameProbeScreen>(false);
	Screen * baseScreen = base.get();
	auto overlay = std::make_unique<FrameProbeScreen>(true);
	Screen * overlayScreen = overlay.get();
	clientUi.PushBuiltScreenForTest(std::move(base));
	clientUi.PushBuiltScreenForTest(std::move(overlay));

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	clientUi.BeginFrame(input);
	clientUi.BuildVisibleScreenFramesForTest(
		[&](silencer::client_ui::UiScreenEntryId entryId,
		    Screen& screen,
		    bool overlayFlag) {
			if(&screen == baseScreen){
				CHECK_FALSE(overlayFlag);
				baseEntry = entryId;
				baseBuilds++;
			}else if(&screen == overlayScreen){
				CHECK(overlayFlag);
				overlayEntry = entryId;
				overlayBuilds++;
			}else{
				FAIL("unexpected visible screen");
			}
		});
	clientUi.EndFrame();

	REQUIRE(baseEntry != 0);
	REQUIRE(overlayEntry != 0);
	Clay_ElementData baseFrame =
		Clay_GetElementData(CLAY_IDI("ClientUiScreenFrame", baseEntry));
	Clay_ElementData overlayFrame =
		Clay_GetElementData(CLAY_IDI("ClientUiOverlayScreenFrame", overlayEntry));
	REQUIRE(baseFrame.found);
	REQUIRE(overlayFrame.found);
	CHECK(baseFrame.boundingBox.x == 0.0f);
	CHECK(baseFrame.boundingBox.y == 0.0f);
	CHECK(baseFrame.boundingBox.width == 640.0f);
	CHECK(baseFrame.boundingBox.height == 480.0f);
	CHECK(overlayFrame.boundingBox.x == 0.0f);
	CHECK(overlayFrame.boundingBox.y == 0.0f);
	CHECK(overlayFrame.boundingBox.width == 640.0f);
	CHECK(overlayFrame.boundingBox.height == 480.0f);
	CHECK(baseBuilds == 1);
	CHECK(overlayBuilds == 1);
}

TEST_CASE("ClientUi overlay focus is isolated from the covered screen") {
	RealClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);

	auto base = std::make_unique<FrameProbeScreen>(false);
	Screen * baseScreen = base.get();
	auto overlay = std::make_unique<FrameProbeScreen>(true);
	Screen * overlayScreen = overlay.get();
	clientUi.PushBuiltScreenForTest(std::move(base));
	clientUi.PushBuiltScreenForTest(std::move(overlay));

	Clay_ElementId baseButton = TestId("CoveredScreenButton");
	Clay_ElementId overlayButton = TestId("OverlayScreenButton");
	silencer::client_ui::UiScreenEntryId overlayEntry = 0;
	auto buildVisible = [&] {
		clientUi.BuildVisibleScreenFramesForTest(
			[&](silencer::client_ui::UiScreenEntryId entryId,
			    Screen& screen,
			    bool) {
				if(&screen == baseScreen){
					RuntimeButton(clientUi.Interactions(), baseButton, "base.activate");
				}else if(&screen == overlayScreen){
					overlayEntry = entryId;
					RuntimeButton(clientUi.Interactions(), overlayButton, "overlay.activate");
				}
			});
	};

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	clientUi.BeginFrame(input);
	buildVisible();
	clientUi.EndFrame();

	REQUIRE(overlayEntry != 0);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(
		             CLAY_IDI("ClientUiScreenFocusScope", 1)),
	             overlayButton));
	CHECK(clientUi.Interactions().FindById("base.activate") == nullptr);
	const auto * overlayElement = clientUi.Interactions().FindById("overlay.activate");
	REQUIRE(overlayElement != nullptr);
	CHECK(overlayElement->focused);
	(void)clientUi.DispatchInput(nullptr, input);

	input.navActions.push_back(silencer::ui::UiNavAction::Confirm);
	input.source = silencer::ui::UiFocusSource::Keyboard;
	clientUi.BeginFrame(input);
	buildVisible();
	clientUi.EndFrame();
	silencer::client_ui::UiDispatchResult result =
		clientUi.DispatchInput(nullptr, input);
	REQUIRE(result.unhandledActions.size() == 1);
	CHECK(result.unhandledActions[0].kind == silencer::ui::UiActionKind::Activate);
	CHECK(result.unhandledActions[0].id == "overlay.activate");
}

TEST_CASE("ClientUi screen focus scopes stay bounded across entry churn") {
	RealClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);

	Clay_ElementId button = TestId("ChurnVisibleButton");
	for(int i = 0; i < 20; ++i){
		auto screen = std::make_unique<FrameProbeScreen>(false);
		Screen * current = screen.get();
		REQUIRE(clientUi.PushBuiltScreenForTest(std::move(screen)));

		silencer::ui::UiInputState input;
		input.width = 640;
		input.height = 480;
		clientUi.BeginFrame(input);
		clientUi.BuildVisibleScreenFramesForTest(
			[&](silencer::client_ui::UiScreenEntryId, Screen& screen, bool) {
				if(&screen == current){
					RuntimeButton(clientUi.Interactions(), button, "churn.visible");
				}
			});
		clientUi.EndFrame();
		(void)clientUi.DispatchInput(nullptr, input);

		CHECK(clientUi.FocusRuntime().errorCount == 0);
		CHECK(clientUi.FocusRuntime().scopeCount <= 2);
		CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(
			             CLAY_IDI("ClientUiScreenFocusScope", 0)),
		             button));
	}
}

TEST_CASE("ClientUi focus supports the maximum visible overlay stack") {
	RealClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);

	auto base = std::make_unique<FrameProbeScreen>(false);
	REQUIRE(clientUi.PushBuiltScreenForTest(std::move(base)));
	Screen * topOverlay = nullptr;
	for(int i = 0; i < 20; ++i){
		auto overlay = std::make_unique<FrameProbeScreen>(true);
		topOverlay = overlay.get();
		REQUIRE(clientUi.PushBuiltScreenForTest(std::move(overlay)));
	}

	auto buildVisible = [&] {
		clientUi.BuildVisibleScreenFramesForTest(
			[&](silencer::client_ui::UiScreenEntryId entryId,
			    Screen& screen,
			    bool) {
				RuntimeButton(
					clientUi.Interactions(),
					CLAY_IDI("ManyOverlayButton", entryId),
					&screen == topOverlay ? "top.overlay" : "covered.overlay");
			});
	};

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	clientUi.BeginFrame(input);
	buildVisible();
	clientUi.EndFrame();
	CHECK(clientUi.FocusRuntime().errorCount == 0);
	const auto * topElement = clientUi.Interactions().FindById("top.overlay");
	REQUIRE(topElement != nullptr);
	CHECK(topElement->focused);
	(void)clientUi.DispatchInput(nullptr, input);

	input.navActions.push_back(silencer::ui::UiNavAction::Confirm);
	input.source = silencer::ui::UiFocusSource::Keyboard;
	clientUi.BeginFrame(input);
	buildVisible();
	clientUi.EndFrame();
	silencer::client_ui::UiDispatchResult result =
		clientUi.DispatchInput(nullptr, input);
	REQUIRE(result.unhandledActions.size() == 1);
	CHECK(result.unhandledActions[0].kind == silencer::ui::UiActionKind::Activate);
	CHECK(result.unhandledActions[0].id == "top.overlay");
	CHECK(clientUi.FocusRuntime().errorCount == 0);
}

TEST_CASE("ClientUi focus sees current frame pointer state before layout") {
	RealClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	Clay_ElementId scope = TestId("ClientUiPointerScope");
	Clay_ElementId first = TestId("ClientUiPointerFirst");
	Clay_ElementId second = TestId("ClientUiPointerSecond");
	int confirmCount = 0;

	auto build = [&] {
		CLAY({
			.id = TestId("ClientUiPointerStack"),
			.layout = {
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
				.childGap = 8,
			},
		}) {
			silencer::ui::ui_focus_push_scope({scope});
			FocusBox(first, false, {}, [&] { confirmCount++; });
			FocusBox(second, false, {}, [&] { confirmCount++; });
			silencer::ui::ui_focus_pop_scope();
		}
	};

	clientUi.BeginFrame(input);
	build();
	clientUi.EndFrame();
	Clay_ElementData firstData = Clay_GetElementData(first);
	Clay_ElementData secondData = Clay_GetElementData(second);
	REQUIRE(firstData.found);
	REQUIRE(secondData.found);

	input.pointer.x = secondData.boundingBox.x + secondData.boundingBox.width * 0.5f;
	input.pointer.y = secondData.boundingBox.y + secondData.boundingBox.height * 0.5f;
	input.pointer.down = true;
	input.pointer.pressed = true;
	clientUi.BeginFrame(input);
	build();
	clientUi.EndFrame();
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), second));
	CHECK(confirmCount == 0);

	input.pointer.pressed = false;
	input.pointer.down = false;
	input.pointer.released = true;
	clientUi.BeginFrame(input);
	build();
	clientUi.EndFrame();
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(scope), second));
	CHECK(confirmCount == 1);
}

TEST_CASE("ClientUi routes registered interactables through focus runtime once") {
	RealClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	Clay_ElementId first = TestId("RuntimeFirst");
	Clay_ElementId second = TestId("RuntimeSecond");
	Clay_ElementId third = TestId("RuntimeThird");

	auto build = [&] {
		CLAY({
			.id = TestId("RuntimeButtonStack"),
			.layout = {
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
				.childGap = 8,
			},
		}) {
			RuntimeButton(clientUi.Interactions(), first, "runtime.first");
			RuntimeButton(clientUi.Interactions(), second, "runtime.second");
			RuntimeButton(clientUi.Interactions(), third, "runtime.third");
		}
	};

	clientUi.BeginFrame(input);
	build();
	clientUi.EndFrame();
	(void)clientUi.DispatchInput(nullptr, input);
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(
	                 CLAY_ID("ClientUiFocusScope")),
	             first));

	input.navActions.push_back(silencer::ui::UiNavAction::Down);
	input.source = silencer::ui::UiFocusSource::Keyboard;
	clientUi.BeginFrame(input);
	build();
	clientUi.EndFrame();

	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(
	                 CLAY_ID("ClientUiFocusScope")),
	             second));
	const auto * secondElement = clientUi.Interactions().FindById("runtime.second");
	REQUIRE(secondElement != nullptr);
	CHECK(secondElement->focused);

	silencer::client_ui::UiDispatchResult result =
		clientUi.DispatchInput(nullptr, input);
	REQUIRE(result.unhandledActions.size() == 1);
	CHECK(result.unhandledActions[0].kind == silencer::ui::UiActionKind::Navigate);
	CHECK(result.unhandledActions[0].id == "runtime.second");
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(
	                 CLAY_ID("ClientUiFocusScope")),
	             second));
	CHECK_FALSE(SameId(silencer::ui::ui_focus_focused_id_for_scope(
	                      CLAY_ID("ClientUiFocusScope")),
	                  third));

	Clay_ElementData secondData = Clay_GetElementData(second);
	REQUIRE(secondData.found);
	input = {};
	input.width = 640;
	input.height = 480;
	input.pointer.x = secondData.boundingBox.x + secondData.boundingBox.width * 0.5f;
	input.pointer.y = secondData.boundingBox.y + secondData.boundingBox.height * 0.5f;
	input.pointer.moved = true;
	input.source = silencer::ui::UiFocusSource::Mouse;
	clientUi.BeginFrame(input);
	build();
	clientUi.EndFrame();
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(
	                 CLAY_ID("ClientUiFocusScope")),
	             second));

	input.pointer.x = 500.0f;
	input.pointer.y = 440.0f;
	clientUi.BeginFrame(input);
	build();
	clientUi.EndFrame();
	CHECK(silencer::ui::ui_focus_focused_id_for_scope(
		      CLAY_ID("ClientUiFocusScope")).id == 0);
	const auto * staleSecond = clientUi.Interactions().FindById("runtime.second");
	REQUIRE(staleSecond != nullptr);
	CHECK_FALSE(staleSecond->focused);
}

TEST_CASE("ClientUi routes focus-next through declaration order instead of spatial down") {
	RealClayBackend backend;
	silencer::ui::ClayService clay(backend);
	silencer::client_ui::ClientUi clientUi(clay);

	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;
	Clay_ElementId first = TestId("SequentialFirst");
	Clay_ElementId second = TestId("SequentialSecond");

	auto build = [&] {
		CLAY({
			.id = TestId("SequentialRow"),
			.layout = {
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
				.childGap = 8,
			},
		}) {
			RuntimeButton(clientUi.Interactions(), first, "sequential.first");
			RuntimeButton(clientUi.Interactions(), second, "sequential.second");
		}
	};

	clientUi.BeginFrame(input);
	build();
	clientUi.EndFrame();
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(
	                 CLAY_ID("ClientUiFocusScope")),
	             first));

	input.navActions.push_back(silencer::ui::UiNavAction::FocusNext);
	input.source = silencer::ui::UiFocusSource::Keyboard;
	clientUi.BeginFrame(input);
	build();
	clientUi.EndFrame();
	CHECK(SameId(silencer::ui::ui_focus_focused_id_for_scope(
	                 CLAY_ID("ClientUiFocusScope")),
	             second));
}
