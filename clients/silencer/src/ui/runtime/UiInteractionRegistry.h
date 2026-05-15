#pragma once

#include "clay/clay.h"
#include "runtime/UiActionQueue.h"
#include "runtime/UiInputState.h"

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
};

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
	bool IsTextInputFocused(int uid) const;
	bool HasFocus() const;
	void ClearFocus();
	bool DispatchTextInput(char ascii);
	bool BackspaceFocusedText();
	bool SubmitFocusedText();
	bool CancelFocused();
	bool PressAt(int x, int y);
	bool FocusNextInteractive();
	bool FocusPreviousInteractive();
	bool FocusDirectional(UiNavAction action);
	bool ActivateFocused();
	void QueueAction(UiAction action);
	std::vector<UiAction> DrainActions();
	void ResolveClayBoundsFromClay();

private:
	bool MatchesFocus(const UiInteractable& widget) const;
	const UiInteractable* FocusedInteractable() const;
	UiInteractable* FocusedInteractable();
	void SetFocus(const UiInteractable& widget);
	void QueueAction(UiActionKind kind, const UiInteractable& widget, const char * value);
	void RefreshElementState();

	std::vector<UiElementSnapshot> elements_;
	std::vector<UiInteractable> interactables_;
	UiActionQueue actions_;
	int focusedUid_ = -1;
	std::string focusedLabel_;
};

}  // namespace ui
}  // namespace silencer
