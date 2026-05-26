#pragma once

#include "clay/clay.h"
#include "runtime/UiActionQueue.h"
#include "runtime/UiInputState.h"

#include <array>
#include <cstddef>
#include <iterator>
#include <string>

namespace silencer {
namespace ui {

constexpr int UI_INTERACTION_MAX_REGISTERED_ELEMENTS = 512;
constexpr int UI_INTERACTION_MAX_INTERACTABLES = 256;
constexpr int UI_INTERACTION_MAX_ELEMENTS =
	UI_INTERACTION_MAX_REGISTERED_ELEMENTS + UI_INTERACTION_MAX_INTERACTABLES;

enum class UiElementKind {
	Container,
	Button,
	Text,
	TextField,
	ListItem,
	Tab,
	Slider,
	Progress,
};

enum class UiInteractableKind {
	Button,
	Toggle,
	TextInput,
	ListRow,
};

struct UiRect {
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};

struct UiElementSnapshot {
	std::string id;
	UiElementKind kind = UiElementKind::Container;
	std::string label;
	std::string value;
	UiRect bounds;
	Clay_ElementId clayId{};
	bool hasClayId = false;
	bool enabled = true;
	bool focused = false;
	bool selected = false;
};

struct UiInteractable {
	std::string id;
	std::string labelText;
	UiInteractableKind kind = UiInteractableKind::Button;
	int uid = -1;
	int x = 0, y = 0, w = 0, h = 0;
	Clay_ElementId clayId{};
	bool hasClayId = false;
	int index = -1;
	bool selected = false;
	std::string value;
	int maxLength = 0;
	bool isPassword = false;
	bool inactive = false;
	bool numbersOnly = false;
	bool cancelOnEscape = false;
	bool requestInitialFocus = false;
	bool requestFocus = false;
};

template <typename T>
class UiConstSpan {
public:
	using const_iterator = const T *;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	UiConstSpan() = default;
	UiConstSpan(const T * items, int count)
		: items_(items), count_(count) {}

	const_iterator begin() const { return items_; }
	const_iterator end() const { return items_ ? items_ + count_ : nullptr; }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
	bool empty() const { return count_ == 0; }
	std::size_t size() const { return static_cast<std::size_t>(count_); }
	int count() const { return count_; }
	const T& operator[](std::size_t index) const { return items_[index]; }

private:
	const T * items_ = nullptr;
	int count_ = 0;
};

using UiElementSnapshotSpan = UiConstSpan<UiElementSnapshot>;
using UiInteractableSpan = UiConstSpan<UiInteractable>;

const char * UiInteractableLabel(const UiInteractable& widget);
bool UiInteractableMatchesLabel(const UiInteractable& widget, const char * label);
bool UiInteractableIsInteractive(const UiInteractable& widget);

class UiInteractionRegistry {
public:
	void BeginFrame();
	bool Register(UiElementSnapshot metadata);
	bool RegisterInteractable(UiInteractable interactable);
	UiElementSnapshotSpan Elements() const;
	UiInteractableSpan Interactables() const;
	int ElementOverflowCount() const { return elementOverflowCount_; }
	int InteractableOverflowCount() const { return interactableOverflowCount_; }
	const UiElementSnapshot* FindById(const std::string& id) const;
	const UiElementSnapshot* FindByLabel(const std::string& label) const;
	const UiInteractable* FindInteractableByLabel(const char * label) const;
	const UiInteractable* FindInteractableByUid(int uid) const;
	const UiInteractable* FindInteractableById(const char * id) const;
	const UiInteractable* FindInteractableById(const char * id, std::size_t len) const;
	const UiElementSnapshot* FindElementForInteractable(const UiInteractable& widget) const;

	bool FocusTextInputAt(int x, int y);
	bool FocusTextInputByUid(int uid);
	void RequestTextInputFocusByUid(int uid);
	bool FocusInteractableById(const char * id);
	bool FocusInteractableById(const char * id, std::size_t len);
	bool IsTextInputFocused(int uid) const;
	bool HasFocus() const;
	bool HasTextInputFocus() const;
	void ClearFocus();
	bool DispatchTextInput(char ascii);
	bool BackspaceFocusedText();
	bool SubmitFocusedText();
	bool CancelFocused();
	bool PressAt(int x, int y);
	bool FocusNextInteractive();
	bool FocusPreviousInteractive();
	bool FocusDirectional(UiNavAction action);
	// Moves pointer-origin focus to the non-text control under a MOVED pointer.
	// Active text inputs keep caret focus through hover, and moving over empty
	// space clears pointer-origin focus without disturbing keyboard/gamepad focus.
	bool FocusHovered(float x, float y);
	bool FocusControlHovered(float x, float y);
	bool ActivateFocused();
	bool QueueAction(UiAction action);
	UiActionList DrainActions();
	int PendingActionCount() const { return actions_.Count(); }
	int ActionOverflowCount() const { return actions_.OverflowCount(); }
	void ResolveClayBoundsFromClay();

private:
	enum class FocusOrigin {
		None,
		Pointer,
		Navigation,
		Text,
	};

	bool MatchesFocus(const UiInteractable& widget) const;
	const UiInteractable* FocusedInteractable() const;
	UiInteractable* FocusedInteractable();
	void SetFocus(const UiInteractable& widget, FocusOrigin origin);
	void RegisterFocusRuntimeTarget(const UiInteractable& widget);
	void FocusFromFocusRuntime(const char * id, std::size_t len);
	void ConfirmFromFocusRuntime(const char * id, std::size_t len);
	bool FocusHoveredAt(float x, float y, bool recordPhysicalSample);
	bool QueueAction(UiActionKind kind, const UiInteractable& widget, const char * value);
	void RefreshElementState();

	std::array<UiElementSnapshot, UI_INTERACTION_MAX_ELEMENTS> elements_ = {};
	std::array<UiElementSnapshot, UI_INTERACTION_MAX_REGISTERED_ELEMENTS> registeredElements_ = {};
	std::array<UiInteractable, UI_INTERACTION_MAX_INTERACTABLES> interactables_ = {};
	UiActionQueue actions_;
	int elementCount_ = 0;
	int registeredElementCount_ = 0;
	int interactableCount_ = 0;
	int elementOverflowCount_ = 0;
	int interactableOverflowCount_ = 0;
	int focusedUid_ = -1;
	UiInteractableKind focusedKind_ = UiInteractableKind::Button;
	UiActionId focusedLabel_;
	FocusOrigin focusedOrigin_ = FocusOrigin::None;
	float hoverSampleX_ = 0.0f;
	float hoverSampleY_ = 0.0f;
	bool haveHoverSample_ = false;
};

}  // namespace ui
}  // namespace silencer
