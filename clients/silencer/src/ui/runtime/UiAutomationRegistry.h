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

enum class UiAutomationWidgetKind {
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

struct UiElementMetadata {
	std::string id;
	UiElementKind kind = UiElementKind::Container;
	std::string label;
	std::string value;
	UiRect bounds;
	bool enabled = true;
	bool focused = false;
	bool selected = false;
};

struct UiAutomationWidget {
	std::string id;
	std::string labelText;
	UiAutomationWidgetKind kind = UiAutomationWidgetKind::Button;
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

class UiAutomationRegistry {
public:
	void BeginFrame();
	void Register(UiElementMetadata metadata);
	void RegisterWidget(UiAutomationWidget widget);
	const std::vector<UiElementMetadata>& Elements() const;
	const std::vector<UiAutomationWidget>& Widgets() const;
	const UiElementMetadata* FindById(const std::string& id) const;
	const UiElementMetadata* FindByLabel(const std::string& label) const;
	const UiAutomationWidget* FindWidgetByLabel(const char * label) const;
	const UiAutomationWidget* FindWidgetByUid(int uid) const;
	const UiAutomationWidget* FindWidgetById(const std::string& id) const;

	bool FocusTextInputAt(int x, int y);
	bool FocusTextInputByUid(int uid);
	bool FocusWidgetById(const std::string& id);
	bool IsTextInputFocused(int uid) const;
	bool HasFocus() const;
	void ClearFocus();
	bool DispatchTextInput(char ascii);
	bool BackspaceFocusedText();
	bool SubmitFocusedText();
	bool CancelFocused();
	bool InvokeAt(int x, int y);
	bool FocusNextInteractive();
	bool FocusPreviousInteractive();
	bool FocusDirectional(UiNavAction action);
	bool ActivateFocused();
	void QueueAction(UiAction action);
	std::vector<UiAction> DrainActions();
	void ResolveClayBoundsFromClay();

private:
	bool MatchesFocus(const UiAutomationWidget& widget) const;
	const UiAutomationWidget* FocusedWidget() const;
	UiAutomationWidget* FocusedWidget();
	void SetFocus(const UiAutomationWidget& widget);
	void QueueAction(UiActionKind kind, const UiAutomationWidget& widget, const char * value);
	void RefreshElementState();

	std::vector<UiElementMetadata> elements_;
	std::vector<UiAutomationWidget> widgets_;
	UiActionQueue actions_;
	int focusedUid_ = -1;
	std::string focusedLabel_;
};

UiAutomationRegistry& ActiveUiAutomationRegistry();

namespace automation {

using WidgetKind = UiAutomationWidgetKind;
using Widget = UiAutomationWidget;

void BeginFrame();
void Register(const Widget& widget);
const std::vector<Widget>& All();
const Widget* FindByLabel(const char * label);
const Widget* FindByUid(int uid);
bool FocusTextInputAt(int x, int y);
bool FocusTextInputByUid(int uid);
bool FocusWidgetById(const std::string& id);
bool IsTextInputFocused(int uid);
bool HasFocus();
void ClearFocus();
bool DispatchTextInput(char ascii);
bool BackspaceFocusedText();
bool SubmitFocusedText();
bool CancelFocused();
bool InvokeAt(int x, int y);
bool FocusNextInteractive();
bool FocusPreviousInteractive();
bool FocusDirectional(UiNavAction action);
bool ActivateFocused();
void QueueAction(UiAction action);
std::vector<UiAction> DrainActions();

}  // namespace automation

}  // namespace ui
}  // namespace silencer
