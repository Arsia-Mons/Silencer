#include "doctest.h"

#include "client/ui/ClientUi.h"
#include "ui/focus/UiFocus.h"
#include "ui/primitives/focusable.h"
#include "ui/runtime/ClayService.h"

#include "clay/clay.h"

#include <cstdlib>
#include <cstring>

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

template <typename Build>
void RunFocusFrame(silencer::ui::UiFocusRuntime& focus,
                   const silencer::ui::UiFocusInputFrame& input,
                   Build build,
                   Clay_Dimensions dimensions = {640.0f, 480.0f},
                   Clay_Vector2 pointer = {-1000.0f, -1000.0f}) {
	EnsureClay();
	silencer::ui::ui_focus_set_current(&focus);
	Clay_SetLayoutDimensions(dimensions);
	Clay_SetPointerState(pointer, input.pointerDown);
	silencer::ui::ui_focus_begin_frame(input);
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
	silencer::ui::ui_focus_end_layout(input);
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
}
