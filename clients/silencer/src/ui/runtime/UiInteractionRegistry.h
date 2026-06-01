#pragma once

#include "runtime/UiActionQueue.h"
#include "runtime/UiInputState.h"
#include "ui/runtime/tree.h"

#include <string>
#include <vector>

namespace silencer {
namespace ui {

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
	bool enabled = true;
	bool focused = false;
	bool selected = false;
};

struct UiInteractable {
	std::string id;
	std::string labelText;
	UiInteractableKind kind = UiInteractableKind::Button;
	::ui::NodeId retainedNodeId = 0;
	int uid = -1;
	int x = 0, y = 0, w = 0, h = 0;
	int index = -1;
	bool selected = false;
	std::string value;
	int maxLength = 0;
	bool isPassword = false;
	bool inactive = false;
	bool numbersOnly = false;
	bool cancelOnEscape = false;
};

const char * UiInteractableLabel(const UiInteractable& widget);
bool UiInteractableMatchesLabel(const UiInteractable& widget, const char * label);
bool UiInteractableIsInteractive(const UiInteractable& widget);

class UiInteractionRegistry {
public:
	void BeginFrame();
	void Register(UiElementSnapshot metadata);
	void RegisterInteractable(UiInteractable interactable);
	const std::vector<UiElementSnapshot>& Elements() const;
	const std::vector<UiInteractable>& Interactables() const;
	const UiElementSnapshot* FindById(const std::string& id) const;
	const UiElementSnapshot* FindByLabel(const std::string& label) const;
	const UiInteractable* FindInteractableByLabel(const char * label) const;
	const UiInteractable* FindInteractableByUid(int uid) const;
	const UiInteractable* FindInteractableById(const std::string& id) const;

	bool FocusTextInputAt(int x, int y);
	bool FocusTextInputByUid(int uid);
	bool FocusInteractableById(const std::string& id);
	bool IsFocused(const UiInteractable& interactable) const;
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
	void QueueAction(UiAction action);
	std::vector<UiAction> DrainActions();

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
	bool FocusHoveredAt(float x, float y, bool recordPhysicalSample);
	void QueueAction(UiActionKind kind, const UiInteractable& widget, const char * value);
	void RefreshElementState();

	std::vector<UiElementSnapshot> elements_;
	std::vector<UiElementSnapshot> registeredElements_;
	std::vector<UiInteractable> interactables_;
	UiActionQueue actions_;
	int focusedUid_ = -1;
	UiInteractableKind focusedKind_ = UiInteractableKind::Button;
	std::string focusedLabel_;
	FocusOrigin focusedOrigin_ = FocusOrigin::None;
	float hoverSampleX_ = 0.0f;
	float hoverSampleY_ = 0.0f;
	bool haveHoverSample_ = false;
};

}  // namespace ui
}  // namespace silencer
