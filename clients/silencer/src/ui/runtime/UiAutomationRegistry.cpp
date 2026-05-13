#include "runtime/UiAutomationRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>

namespace silencer {
namespace ui {

namespace {

bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
	if(a.size() != b.size()) return false;
	for(size_t i = 0; i < a.size(); ++i) {
		char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
		char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
		if(ca != cb) return false;
	}
	return true;
}

bool LabelEquals(const char * a, const char * b) {
	if(!a || !b) return false;
	while(*a && *b){
		if(std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
		++a;
		++b;
	}
	return *a == 0 && *b == 0;
}

bool PointIn(const UiAutomationWidget& widget, int x, int y) {
	return x >= widget.x && y >= widget.y
	    && x < widget.x + widget.w && y < widget.y + widget.h;
}

bool IsInteractive(const UiAutomationWidget& widget) {
	if(widget.inactive) return false;
	return widget.kind == UiAutomationWidgetKind::Button ||
	       widget.kind == UiAutomationWidgetKind::Toggle ||
	       widget.kind == UiAutomationWidgetKind::ListRow ||
	       widget.kind == UiAutomationWidgetKind::TextInput;
}

UiElementKind MetadataKind(UiAutomationWidgetKind kind) {
	switch(kind){
		case UiAutomationWidgetKind::Button: return UiElementKind::Button;
		case UiAutomationWidgetKind::Toggle: return UiElementKind::Button;
		case UiAutomationWidgetKind::TextInput: return UiElementKind::TextField;
		case UiAutomationWidgetKind::ListRow: return UiElementKind::ListItem;
	}
	return UiElementKind::Container;
}

}  // namespace

void UiAutomationRegistry::BeginFrame() {
	elements_.clear();
	widgets_.clear();
}

void UiAutomationRegistry::Register(UiElementMetadata metadata) {
	elements_.push_back(std::move(metadata));
}

void UiAutomationRegistry::RegisterWidget(UiAutomationWidget widget) {
	widgets_.push_back(widget);

	UiElementMetadata metadata;
	if(widget.uid >= 0) metadata.id = std::to_string(widget.uid);
	else if(widget.label) metadata.id = widget.label;
	metadata.kind = MetadataKind(widget.kind);
	if(widget.label) metadata.label = widget.label;
	if(widget.textBuffer){
		metadata.value = widget.isPassword
			? std::string(std::strlen(widget.textBuffer), '*')
			: std::string(widget.textBuffer);
	}
	metadata.bounds = UiRect{
		static_cast<float>(widget.x),
		static_cast<float>(widget.y),
		static_cast<float>(widget.w),
		static_cast<float>(widget.h),
	};
	metadata.enabled = !widget.inactive;
	metadata.focused = MatchesFocus(widget);
	metadata.selected = widget.selected;
	elements_.push_back(std::move(metadata));
}

const std::vector<UiElementMetadata>& UiAutomationRegistry::Elements() const {
	return elements_;
}

const std::vector<UiAutomationWidget>& UiAutomationRegistry::Widgets() const {
	return widgets_;
}

const UiElementMetadata* UiAutomationRegistry::FindById(const std::string& id) const {
	auto it = std::find_if(elements_.begin(), elements_.end(), [&](const UiElementMetadata& element) {
		return element.id == id;
	});
	return it == elements_.end() ? nullptr : &*it;
}

const UiElementMetadata* UiAutomationRegistry::FindByLabel(const std::string& label) const {
	auto it = std::find_if(elements_.begin(), elements_.end(), [&](const UiElementMetadata& element) {
		return EqualsIgnoreCase(element.label, label);
	});
	return it == elements_.end() ? nullptr : &*it;
}

const UiAutomationWidget* UiAutomationRegistry::FindWidgetByLabel(const char * label) const {
	if(!label || !*label) return nullptr;
	const UiAutomationWidget * hit = nullptr;
	int count = 0;
	for(const auto& widget : widgets_){
		if(widget.label && LabelEquals(widget.label, label)){
			hit = &widget;
			++count;
		}
	}
	return count == 1 ? hit : nullptr;
}

const UiAutomationWidget* UiAutomationRegistry::FindWidgetByUid(int uid) const {
	const UiAutomationWidget * hit = nullptr;
	int count = 0;
	for(const auto& widget : widgets_){
		if(widget.uid == uid){
			hit = &widget;
			++count;
		}
	}
	return count == 1 ? hit : nullptr;
}

bool UiAutomationRegistry::MatchesFocus(const UiAutomationWidget& widget) const {
	if(focusedUid_ >= 0 && widget.uid == focusedUid_) return true;
	return focusedUid_ < 0 && !focusedLabel_.empty() && widget.label
	    && LabelEquals(widget.label, focusedLabel_.c_str());
}

const UiAutomationWidget* UiAutomationRegistry::FocusedWidget() const {
	for(const auto& widget : widgets_){
		if(MatchesFocus(widget)) return &widget;
	}
	return nullptr;
}

void UiAutomationRegistry::SetFocus(const UiAutomationWidget& widget) {
	focusedUid_ = widget.uid;
	focusedLabel_ = widget.label ? widget.label : "";
}

void UiAutomationRegistry::QueueAction(UiActionKind kind,
                                       const UiAutomationWidget& widget,
                                       const char * value) {
	UiAction action;
	action.kind = kind;
	if(widget.uid >= 0) action.id = std::to_string(widget.uid);
	else if(widget.label) action.id = widget.label;
	if(value) action.value = value;
	else if(widget.label) action.value = widget.label;
	if(kind == UiActionKind::Activate){
		if(widget.kind == UiAutomationWidgetKind::TextInput){
			action.onEnter = widget.onEnter;
			action.enterUser = widget.enterUser;
		}else{
			action.onClick = widget.onClick;
			action.clickUser = widget.clickUser;
		}
	}else if(kind == UiActionKind::Select && widget.kind == UiAutomationWidgetKind::ListRow){
		action.onClickRow = widget.onClickRow;
		action.clickUser = widget.clickUser;
		action.rowIndex = widget.rowIndex;
	}
	actions_.Push(std::move(action));
}

bool UiAutomationRegistry::FocusTextInputAt(int x, int y) {
	for(auto it = widgets_.rbegin(); it != widgets_.rend(); ++it){
		if(it->kind == UiAutomationWidgetKind::TextInput &&
		   !it->inactive && PointIn(*it, x, y)){
			SetFocus(*it);
			QueueAction(UiActionKind::Select, *it, it->textBuffer ? it->textBuffer : "");
			return true;
		}
	}
	return false;
}

bool UiAutomationRegistry::FocusTextInputByUid(int uid) {
	for(const auto& widget : widgets_){
		if(widget.kind == UiAutomationWidgetKind::TextInput &&
		   widget.uid == uid && !widget.inactive){
			SetFocus(widget);
			QueueAction(UiActionKind::Select, widget, widget.textBuffer ? widget.textBuffer : "");
			return true;
		}
	}
	return false;
}

bool UiAutomationRegistry::IsTextInputFocused(int uid) const {
	const UiAutomationWidget * widget = FocusedWidget();
	return widget && widget->uid == uid;
}

void UiAutomationRegistry::ClearFocus() {
	focusedUid_ = -1;
	focusedLabel_.clear();
}

bool UiAutomationRegistry::DispatchTextInput(char ascii) {
	const UiAutomationWidget * widget = FocusedWidget();
	if(!widget || !widget->textBuffer || widget->textBufferLen <= 0 || widget->inactive) return false;
	if(widget->numbersOnly && (ascii < '0' || ascii > '9')) return false;
	if(ascii < 0x20 || ascii > 0x7E) return false;
	switch(ascii){
		case '[':
		case '\\':
		case ']':
		case '^':
		case '_':
		case '`':
		case '{':
		case '|':
		case '}':
		case '~':
			return false;
	}
	int len = static_cast<int>(std::strlen(widget->textBuffer));
	if(len >= widget->textBufferLen - 1) return true;
	widget->textBuffer[len] = ascii;
	widget->textBuffer[len + 1] = '\0';
	QueueAction(UiActionKind::SetText, *widget, widget->textBuffer);
	return true;
}

bool UiAutomationRegistry::DispatchKeyPress(char ascii) {
	const UiAutomationWidget * widget = FocusedWidget();
	switch(ascii){
		case '\b':
			if(!widget || widget->kind != UiAutomationWidgetKind::TextInput ||
			   !widget->textBuffer || widget->textBufferLen <= 0 || widget->inactive){
				return false;
			}
			{
				int len = static_cast<int>(std::strlen(widget->textBuffer));
				if(len > 0) widget->textBuffer[len - 1] = '\0';
			}
			QueueAction(UiActionKind::SetText, *widget, widget->textBuffer);
			return true;
		case 2:
		case '\t':
		case 4:
			return FocusNextInteractive();
		case 1:
		case 3:
			return FocusPreviousInteractive();
		case '\n':
			if(widget && widget->kind == UiAutomationWidgetKind::TextInput && !widget->inactive){
				QueueAction(UiActionKind::Activate, *widget, widget->textBuffer);
				return true;
			}
			return false;
		case 0x1B:
			ClearFocus();
			return false;
		default:
			return false;
	}
}

bool UiAutomationRegistry::InvokeAt(int x, int y) {
	for(auto it = widgets_.rbegin(); it != widgets_.rend(); ++it){
		if(IsInteractive(*it) && PointIn(*it, x, y)){
			SetFocus(*it);
			switch(it->kind){
				case UiAutomationWidgetKind::Button:
				case UiAutomationWidgetKind::Toggle:
					QueueAction(UiActionKind::Activate, *it, nullptr);
					return it->onClick != nullptr;
				case UiAutomationWidgetKind::ListRow:
					QueueAction(UiActionKind::Select, *it, nullptr);
					return it->onClickRow != nullptr;
				case UiAutomationWidgetKind::TextInput:
					QueueAction(UiActionKind::Select, *it, it->textBuffer ? it->textBuffer : "");
					return true;
			}
		}
	}
	ClearFocus();
	return false;
}

bool UiAutomationRegistry::FocusNextInteractive() {
	std::vector<const UiAutomationWidget *> items;
	for(const auto& widget : widgets_){
		if(IsInteractive(widget)) items.push_back(&widget);
	}
	if(items.empty()) return false;
	int current = -1;
	for(int i = 0; i < static_cast<int>(items.size()); ++i){
		if(MatchesFocus(*items[i])){
			current = i;
			break;
		}
	}
	SetFocus(*items[(current + 1) % static_cast<int>(items.size())]);
	QueueAction(UiActionKind::Navigate, *items[(current + 1) % static_cast<int>(items.size())], "focus_next");
	return true;
}

bool UiAutomationRegistry::FocusPreviousInteractive() {
	std::vector<const UiAutomationWidget *> items;
	for(const auto& widget : widgets_){
		if(IsInteractive(widget)) items.push_back(&widget);
	}
	if(items.empty()) return false;
	int current = 0;
	for(int i = 0; i < static_cast<int>(items.size()); ++i){
		if(MatchesFocus(*items[i])){
			current = i;
			break;
		}
	}
	int next = current - 1;
	if(next < 0) next = static_cast<int>(items.size()) - 1;
	SetFocus(*items[next]);
	QueueAction(UiActionKind::Navigate, *items[next], "focus_previous");
	return true;
}

bool UiAutomationRegistry::ActivateFocused() {
	const UiAutomationWidget * widget = FocusedWidget();
	if(!widget || widget->inactive) return false;
	switch(widget->kind){
		case UiAutomationWidgetKind::Button:
		case UiAutomationWidgetKind::Toggle:
			QueueAction(UiActionKind::Activate, *widget, nullptr);
			return widget->onClick != nullptr;
		case UiAutomationWidgetKind::ListRow:
			QueueAction(UiActionKind::Select, *widget, nullptr);
			return widget->onClickRow != nullptr;
		case UiAutomationWidgetKind::TextInput:
			QueueAction(UiActionKind::Select, *widget, widget->textBuffer ? widget->textBuffer : "");
			return true;
	}
	return false;
}

void UiAutomationRegistry::QueueAction(UiAction action) {
	actions_.Push(std::move(action));
}

std::vector<UiAction> UiAutomationRegistry::DrainActions() {
	return actions_.Drain();
}

UiAutomationRegistry& ActiveUiAutomationRegistry() {
	static UiAutomationRegistry registry;
	return registry;
}

namespace automation {

void BeginFrame() { ActiveUiAutomationRegistry().BeginFrame(); }
void Register(const Widget& widget) { ActiveUiAutomationRegistry().RegisterWidget(widget); }
const std::vector<Widget>& All() { return ActiveUiAutomationRegistry().Widgets(); }
const Widget* FindByLabel(const char * label) { return ActiveUiAutomationRegistry().FindWidgetByLabel(label); }
const Widget* FindByUid(int uid) { return ActiveUiAutomationRegistry().FindWidgetByUid(uid); }
bool FocusTextInputAt(int x, int y) { return ActiveUiAutomationRegistry().FocusTextInputAt(x, y); }
bool FocusTextInputByUid(int uid) { return ActiveUiAutomationRegistry().FocusTextInputByUid(uid); }
bool IsTextInputFocused(int uid) { return ActiveUiAutomationRegistry().IsTextInputFocused(uid); }
void ClearFocus() { ActiveUiAutomationRegistry().ClearFocus(); }
bool DispatchTextInput(char ascii) { return ActiveUiAutomationRegistry().DispatchTextInput(ascii); }
bool DispatchKeyPress(char ascii) { return ActiveUiAutomationRegistry().DispatchKeyPress(ascii); }
bool InvokeAt(int x, int y) { return ActiveUiAutomationRegistry().InvokeAt(x, y); }
bool FocusNextInteractive() { return ActiveUiAutomationRegistry().FocusNextInteractive(); }
bool FocusPreviousInteractive() { return ActiveUiAutomationRegistry().FocusPreviousInteractive(); }
bool ActivateFocused() { return ActiveUiAutomationRegistry().ActivateFocused(); }
void QueueAction(UiAction action) { ActiveUiAutomationRegistry().QueueAction(std::move(action)); }
void QueueClick(std::string id, void (*onClick)(void *), void * user) {
	UiAction action;
	action.kind = UiActionKind::Activate;
	action.id = std::move(id);
	action.value = action.id;
	action.onClick = onClick;
	action.clickUser = user;
	ActiveUiAutomationRegistry().QueueAction(std::move(action));
}
void QueueRowSelect(std::string id, int rowIndex, void (*onClickRow)(void *, int), void * user) {
	UiAction action;
	action.kind = UiActionKind::Select;
	action.id = std::move(id);
	action.value = std::to_string(rowIndex);
	action.onClickRow = onClickRow;
	action.clickUser = user;
	action.rowIndex = rowIndex;
	ActiveUiAutomationRegistry().QueueAction(std::move(action));
}
void QueueTextEnter(std::string id, const char * value, void (*onEnter)(void *), void * user) {
	UiAction action;
	action.kind = UiActionKind::Activate;
	action.id = std::move(id);
	action.value = value ? value : "";
	action.onEnter = onEnter;
	action.enterUser = user;
	ActiveUiAutomationRegistry().QueueAction(std::move(action));
}
std::vector<UiAction> DrainActions() { return ActiveUiAutomationRegistry().DrainActions(); }

}  // namespace automation

}  // namespace ui
}  // namespace silencer
