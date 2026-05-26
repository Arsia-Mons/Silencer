#pragma once

#include "clay/clay.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>

namespace silencer {
namespace ui {

template <typename T>
struct Span {
	T * items = nullptr;
	int count = 0;

	T * begin() const { return items; }
	T * end() const { return items + count; }
	T& operator[](int index) const { return items[index]; }
};

struct UiRuntimeLimits {
	int maxFocusScopes = 33;
	int maxFocusablesPerScope = 256;
	int maxUiIntents = 128;
	int maxScreens = 32;
};

enum class UiNavDir {
	Up,
	Down,
	Left,
	Right,
};

enum class UiFocusSource {
	None,
	Keyboard,
	Gamepad,
	Mouse,
	Touch,
	Programmatic,
};

enum class UiNavRuleKind {
	Auto,
	Stop,
	Wrap,
	Explicit,
};

struct UiNavRule {
	UiNavRuleKind kind = UiNavRuleKind::Auto;
	Clay_ElementId explicitTarget = {};
};

struct UiNavRules {
	UiNavRule up;
	UiNavRule down;
	UiNavRule left;
	UiNavRule right;
};

struct UiFocusInputFrame {
	bool navUp = false;
	bool navDown = false;
	bool navLeft = false;
	bool navRight = false;
	bool focusNext = false;
	bool focusPrevious = false;

	bool confirmPressed = false;
	bool confirmDown = false;
	bool confirmReleased = false;

	bool cancelPressed = false;
	bool cancelDown = false;
	bool cancelReleased = false;

	bool pointerPressed = false;
	bool pointerDown = false;
	bool pointerReleased = false;
	bool pointerMoved = false;
	float pointerX = 0.0f;
	float pointerY = 0.0f;

	UiFocusSource source = UiFocusSource::Keyboard;
};

struct UiFocusScopeDesc {
	Clay_ElementId id = {};
	bool modal = false;
	bool wrap = false;
};

struct UiFocusableDesc {
	Clay_ElementId id = {};
	bool disabled = false;
	UiNavRules nav = {};
	std::function<void()> onConfirm = {};
	std::function<void()> onFocus = {};
};

struct UiFocusableState {
	Clay_ElementId id = {};
	bool focused = false;
	bool focusVisible = false;
	bool hovered = false;
	bool pressed = false;
	bool disabled = false;
};

constexpr int UI_FOCUS_MAX_SCOPES = 33;
constexpr int UI_FOCUS_MAX_FOCUSABLES_PER_SCOPE = 256;

struct UiFocusableLayout {
	Clay_ElementId id = {};
	Clay_BoundingBox rect = {};
	bool disabled = false;
	uint32_t order = 0;
	UiNavRules nav = {};
};

struct UiFocusableRegistration {
	Clay_ElementId id = {};
	bool disabled = false;
	UiNavRules nav = {};
	std::function<void()> onConfirm = {};
	std::function<void()> onFocus = {};
};

struct UiFocusScope {
	Clay_ElementId id = {};
	bool modal = false;
	bool wrap = false;
	Clay_ElementId focusedId = {};
	Clay_ElementId pointerPressOrigin = {};
	UiFocusSource source = UiFocusSource::None;
	Clay_ElementId requestedInitialFocus = {};
	bool pointerClearedFocus = false;
	bool autoFocusSuppressed = false;
	uint32_t declaredFrame = 0;
	uint32_t declarationOrder = 0;

	std::array<UiFocusableRegistration, UI_FOCUS_MAX_FOCUSABLES_PER_SCOPE> pending = {};
	int pendingCount = 0;

	std::array<UiFocusableLayout, UI_FOCUS_MAX_FOCUSABLES_PER_SCOPE> layout = {};
	int layoutCount = 0;
};

using UiFocusScopeStorage = std::array<UiFocusScope, UI_FOCUS_MAX_SCOPES>;

struct UiFocusRuntime {
	UiRuntimeLimits limits = {};
	std::unique_ptr<UiFocusScopeStorage> scopesStorage = {};
	UiFocusScope * scopes = nullptr;
	int scopeCount = 0;

	int scopeStack[UI_FOCUS_MAX_SCOPES] = {};
	int scopeStackCount = 0;
	int scopeNoopPushDepth = 0;

	uint32_t frame = 0;
	uint32_t nextDeclarationOrder = 0;
	int errorCount = 0;

	Clay_ElementId pendingFocusCallbackId = {};
	Clay_ElementId pendingPointerConfirmId = {};
	bool pointerDown = false;
};

void ui_focus_init(UiFocusRuntime * runtime, UiRuntimeLimits limits = {});
void ui_focus_set_current(UiFocusRuntime * runtime);
UiFocusRuntime * ui_focus_current();

void ui_focus_begin_frame(const UiFocusInputFrame& input);
void ui_focus_end_layout(const UiFocusInputFrame& input = {});

void ui_focus_push_scope(const UiFocusScopeDesc& desc);
void ui_focus_pop_scope();
void ui_focus_request_initial_focus(Clay_ElementId id);

UiFocusableState ui_focusable(const UiFocusableDesc& desc);

Clay_ElementId ui_focus_focused_id();
Clay_ElementId ui_focus_focused_id_for_scope(Clay_ElementId scopeId);
UiFocusSource ui_focus_source();
UiFocusSource ui_focus_source_for_scope(Clay_ElementId scopeId);
int ui_focus_error_count();

}  // namespace ui
}  // namespace silencer
