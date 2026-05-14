#include "runtime/UiAutomationRegistry.h"

#include "runtime/UiTextPolicy.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
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

const char * WidgetLabel(const UiAutomationWidget& widget) {
	if(!widget.labelText.empty()) return widget.labelText.c_str();
	return widget.label;
}

std::string WidgetId(const UiAutomationWidget& widget) {
	if(!widget.id.empty()) return widget.id;
	if(widget.uid >= 0) return std::to_string(widget.uid);
	const char * label = WidgetLabel(widget);
	return label ? std::string(label) : std::string();
}

bool PointIn(const UiAutomationWidget& widget, int x, int y) {
	return x >= widget.x && y >= widget.y
	    && x < widget.x + widget.w && y < widget.y + widget.h;
}

bool HasBounds(const UiAutomationWidget& widget) {
	return widget.w > 0 && widget.h > 0;
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

UiElementMetadata MetadataFromWidget(const UiAutomationWidget& widget, bool focused) {
	UiElementMetadata metadata;
	metadata.id = WidgetId(widget);
	metadata.kind = MetadataKind(widget.kind);
	const char * label = WidgetLabel(widget);
	if(label) metadata.label = label;
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
	metadata.focused = focused;
	metadata.selected = widget.selected;
	return metadata;
}

float CenterX(const UiAutomationWidget& widget) {
	return static_cast<float>(widget.x) + static_cast<float>(widget.w) * 0.5f;
}

float CenterY(const UiAutomationWidget& widget) {
	return static_cast<float>(widget.y) + static_cast<float>(widget.h) * 0.5f;
}

bool IsDirectionalCandidate(const UiAutomationWidget& from,
                            const UiAutomationWidget& to,
                            UiNavAction action,
                            float& primary,
                            float& secondary) {
	const float dx = CenterX(to) - CenterX(from);
	const float dy = CenterY(to) - CenterY(from);
	switch(action){
		case UiNavAction::Up:
			if(dy >= -0.5f) return false;
			primary = -dy;
			secondary = std::fabs(dx);
			return true;
		case UiNavAction::Down:
			if(dy <= 0.5f) return false;
			primary = dy;
			secondary = std::fabs(dx);
			return true;
		case UiNavAction::Left:
			if(dx >= -0.5f) return false;
			primary = -dx;
			secondary = std::fabs(dy);
			return true;
		case UiNavAction::Right:
			if(dx <= 0.5f) return false;
			primary = dx;
			secondary = std::fabs(dy);
			return true;
		default:
			break;
	}
	return false;
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
	const std::string incomingId = WidgetId(widget);
	UiAutomationWidget * existing = nullptr;
	if(!incomingId.empty()){
		for(auto& candidate : widgets_){
			if(WidgetId(candidate) == incomingId){
				existing = &candidate;
				break;
			}
		}
	}

	if(existing){
		if(widget.hasClayId){
			existing->clayId = widget.clayId;
			existing->hasClayId = true;
		}
		if(HasBounds(widget)){
			existing->x = widget.x;
			existing->y = widget.y;
			existing->w = widget.w;
			existing->h = widget.h;
		}
		if(!widget.id.empty()) existing->id = widget.id;
		if(!widget.labelText.empty()) existing->labelText = widget.labelText;
		if(widget.label) existing->label = widget.label;
		existing->kind = widget.kind;
		if(widget.uid >= 0) existing->uid = widget.uid;
		if(widget.onClick) {
			existing->onClick = widget.onClick;
			existing->clickUser = widget.clickUser;
		}
		if(widget.onClickRow) {
			existing->onClickRow = widget.onClickRow;
			existing->clickUser = widget.clickUser;
			existing->rowIndex = widget.rowIndex;
		}else if(widget.rowIndex >= 0){
			existing->rowIndex = widget.rowIndex;
		}
		if(widget.textBuffer){
			existing->textBuffer = widget.textBuffer;
			existing->textBufferLen = widget.textBufferLen;
			existing->isPassword = widget.isPassword;
			existing->numbersOnly = widget.numbersOnly;
		}
		if(widget.onEnter){
			existing->onEnter = widget.onEnter;
			existing->enterUser = widget.enterUser;
		}
		if(widget.onCancel){
			existing->onCancel = widget.onCancel;
			existing->cancelUser = widget.cancelUser;
		}
		existing->selected = widget.selected;
		existing->inactive = widget.inactive;
	}else{
		if(widget.id.empty()) widget.id = incomingId;
		widgets_.push_back(std::move(widget));
	}

	RefreshElementState();
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
		const char * widgetLabel = WidgetLabel(widget);
		if(widgetLabel && LabelEquals(widgetLabel, label)){
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

const UiAutomationWidget* UiAutomationRegistry::FindWidgetById(const std::string& id) const {
	for(const auto& widget : widgets_){
		if(WidgetId(widget) == id) return &widget;
	}
	return nullptr;
}

bool UiAutomationRegistry::MatchesFocus(const UiAutomationWidget& widget) const {
	if(focusedUid_ >= 0 && widget.uid == focusedUid_) return true;
	if(!focusedLabel_.empty() && WidgetId(widget) == focusedLabel_) return true;
	const char * label = WidgetLabel(widget);
	return focusedUid_ < 0 && !focusedLabel_.empty() && label
	    && LabelEquals(label, focusedLabel_.c_str());
}

const UiAutomationWidget* UiAutomationRegistry::FocusedWidget() const {
	for(const auto& widget : widgets_){
		if(MatchesFocus(widget)) return &widget;
	}
	return nullptr;
}

void UiAutomationRegistry::SetFocus(const UiAutomationWidget& widget) {
	focusedUid_ = widget.uid;
	focusedLabel_ = WidgetId(widget);
	RefreshElementState();
}

void UiAutomationRegistry::QueueAction(UiActionKind kind,
                                       const UiAutomationWidget& widget,
                                       const char * value) {
	UiAction action;
	action.kind = kind;
	action.id = WidgetId(widget);
	if(value) action.value = value;
	else{
		const char * label = WidgetLabel(widget);
		if(label) action.value = label;
	}
	action.index = widget.rowIndex;
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

bool UiAutomationRegistry::FocusWidgetById(const std::string& id) {
	const UiAutomationWidget * widget = FindWidgetById(id);
	if(!widget || !IsInteractive(*widget)) return false;
	SetFocus(*widget);
	return true;
}

bool UiAutomationRegistry::IsTextInputFocused(int uid) const {
	const UiAutomationWidget * widget = FocusedWidget();
	return widget && widget->uid == uid;
}

bool UiAutomationRegistry::HasFocus() const {
	return FocusedWidget() != nullptr;
}

void UiAutomationRegistry::ClearFocus() {
	focusedUid_ = -1;
	focusedLabel_.clear();
	RefreshElementState();
}

bool UiAutomationRegistry::DispatchTextInput(char ascii) {
	const UiAutomationWidget * widget = FocusedWidget();
	if(!widget || !widget->textBuffer || widget->textBufferLen <= 0 || widget->inactive) return false;
	UiTextPolicy policy;
	policy.numbersOnly = widget->numbersOnly;
	policy.maxLength = widget->textBufferLen - 1;
	if(!UiTextAllowsChar(ascii, policy)) return false;
	std::string text(widget->textBuffer);
	if(!UiTextAppend(text, ascii, policy)) return false;
	const int len = std::min(static_cast<int>(text.size()), widget->textBufferLen - 1);
	std::memcpy(widget->textBuffer, text.data(), len);
	widget->textBuffer[len] = '\0';
	QueueAction(UiActionKind::SetText, *widget, widget->textBuffer);
	RefreshElementState();
	return true;
}

bool UiAutomationRegistry::BackspaceFocusedText() {
	const UiAutomationWidget * widget = FocusedWidget();
	if(!widget || widget->kind != UiAutomationWidgetKind::TextInput ||
	   !widget->textBuffer || widget->textBufferLen <= 0 || widget->inactive){
		return false;
	}
	std::string text(widget->textBuffer);
	UiTextBackspace(text);
	const int len = std::min(static_cast<int>(text.size()), widget->textBufferLen - 1);
	std::memcpy(widget->textBuffer, text.data(), len);
	widget->textBuffer[len] = '\0';
	QueueAction(UiActionKind::SetText, *widget, widget->textBuffer);
	RefreshElementState();
	return true;
}

bool UiAutomationRegistry::SubmitFocusedText() {
	const UiAutomationWidget * widget = FocusedWidget();
	if(widget && widget->kind == UiAutomationWidgetKind::TextInput && !widget->inactive){
		QueueAction(UiActionKind::SubmitText, *widget, widget->textBuffer);
		return true;
	}
	return false;
}

bool UiAutomationRegistry::CancelFocused() {
	const UiAutomationWidget * widget = FocusedWidget();
	if(widget && widget->kind == UiAutomationWidgetKind::TextInput && !widget->inactive){
		if(widget->onCancel) QueueAction(UiActionKind::Cancel, *widget, widget->textBuffer);
		ClearFocus();
		return true;
	}
	return false;
}

bool UiAutomationRegistry::InvokeAt(int x, int y) {
	for(auto it = widgets_.rbegin(); it != widgets_.rend(); ++it){
		if(IsInteractive(*it) && PointIn(*it, x, y)){
			SetFocus(*it);
			switch(it->kind){
				case UiAutomationWidgetKind::Button:
				case UiAutomationWidgetKind::Toggle:
					QueueAction(UiActionKind::Activate, *it, nullptr);
					return true;
				case UiAutomationWidgetKind::ListRow:
					QueueAction(UiActionKind::Select, *it, "pointer");
					return true;
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
	int next = (current + 1) % static_cast<int>(items.size());
	SetFocus(*items[next]);
	QueueAction(UiActionKind::Navigate, *items[next], "focus_next");
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

bool UiAutomationRegistry::FocusDirectional(UiNavAction action) {
	const UiAutomationWidget * focused = FocusedWidget();
	if(!focused || !HasBounds(*focused)) {
		if(action == UiNavAction::Down || action == UiNavAction::Right) return FocusNextInteractive();
		if(action == UiNavAction::Up || action == UiNavAction::Left) return FocusPreviousInteractive();
		return false;
	}

	const UiAutomationWidget * best = nullptr;
	float bestPrimary = 0.0f;
	float bestSecondary = 0.0f;
	for(const auto& widget : widgets_){
		if(&widget == focused || !IsInteractive(widget) || !HasBounds(widget)) continue;
		float primary = 0.0f;
		float secondary = 0.0f;
		if(!IsDirectionalCandidate(*focused, widget, action, primary, secondary)) continue;
		if(!best || secondary < bestSecondary ||
		   (secondary == bestSecondary && primary < bestPrimary)){
			best = &widget;
			bestPrimary = primary;
			bestSecondary = secondary;
		}
	}
	if(!best) return false;
	SetFocus(*best);
	switch(action){
		case UiNavAction::Up: QueueAction(UiActionKind::Navigate, *best, "up"); break;
		case UiNavAction::Down: QueueAction(UiActionKind::Navigate, *best, "down"); break;
		case UiNavAction::Left: QueueAction(UiActionKind::Navigate, *best, "left"); break;
		case UiNavAction::Right: QueueAction(UiActionKind::Navigate, *best, "right"); break;
		default: break;
	}
	return true;
}

bool UiAutomationRegistry::ActivateFocused() {
	const UiAutomationWidget * widget = FocusedWidget();
	if(!widget || widget->inactive) return false;
	switch(widget->kind){
		case UiAutomationWidgetKind::Button:
		case UiAutomationWidgetKind::Toggle:
			QueueAction(UiActionKind::Activate, *widget, nullptr);
			return true;
		case UiAutomationWidgetKind::ListRow:
			QueueAction(UiActionKind::Select, *widget, "activate");
			return true;
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

bool UiAutomationRegistry::DispatchAction(const UiAction& action) {
	const UiAutomationWidget * widget = nullptr;
	if(!action.id.empty()) widget = FindWidgetById(action.id);
	if(!widget && !action.id.empty()) widget = FindWidgetByLabel(action.id.c_str());
	if(!widget && !action.id.empty()){
		char * end = nullptr;
		long uid = std::strtol(action.id.c_str(), &end, 10);
		if(end && *end == '\0') widget = FindWidgetByUid(static_cast<int>(uid));
	}
	if(!widget && action.index >= 0){
		for(const auto& candidate : widgets_){
			if(candidate.rowIndex == action.index){
				widget = &candidate;
				break;
			}
		}
	}
	if(!widget) return false;

	switch(action.kind){
		case UiActionKind::Activate:
			if(widget->kind == UiAutomationWidgetKind::TextInput){
				if(widget->onEnter) widget->onEnter(widget->enterUser);
				return widget->onEnter != nullptr;
			}
			if(widget->onClick) widget->onClick(widget->clickUser);
			return widget->onClick != nullptr;
		case UiActionKind::Select:
			if(widget->kind == UiAutomationWidgetKind::ListRow){
				if(widget->onClickRow) {
					widget->onClickRow(widget->clickUser, action.index >= 0 ? action.index : widget->rowIndex);
					return true;
				}
			}
			if(widget->kind == UiAutomationWidgetKind::TextInput){
				SetFocus(*widget);
				return true;
			}
			if(widget->onClick) {
				widget->onClick(widget->clickUser);
				return true;
			}
			return false;
		case UiActionKind::SetText:
			if(widget->kind == UiAutomationWidgetKind::TextInput &&
			   widget->textBuffer && widget->textBufferLen > 0){
				int len = static_cast<int>(action.value.size());
				if(len > widget->textBufferLen - 1) len = widget->textBufferLen - 1;
				std::memcpy(widget->textBuffer, action.value.data(), len);
				widget->textBuffer[len] = '\0';
				RefreshElementState();
				return true;
			}
			return false;
		case UiActionKind::SubmitText:
			if(widget->kind == UiAutomationWidgetKind::TextInput && widget->onEnter){
				widget->onEnter(widget->enterUser);
				return true;
			}
			return false;
		case UiActionKind::Cancel:
			if(widget->onCancel){
				widget->onCancel(widget->cancelUser);
				return true;
			}
			return false;
		case UiActionKind::Scroll:
			if(widget->onClick){
				int amount = action.amount > 0 ? action.amount : 1;
				for(int i = 0; i < amount; ++i) widget->onClick(widget->clickUser);
				return true;
			}
			return false;
		case UiActionKind::None:
		case UiActionKind::Navigate:
			return false;
	}
	return false;
}

void UiAutomationRegistry::DispatchActions(const std::vector<UiAction>& actions) {
	for(const UiAction& action : actions){
		DispatchAction(action);
	}
}

void UiAutomationRegistry::ResolveClayBoundsFromClay() {
	for(auto& widget : widgets_){
		if(!widget.hasClayId) continue;
		Clay_ElementData data = Clay_GetElementData(widget.clayId);
		if(!data.found) continue;
		widget.x = static_cast<int>(data.boundingBox.x);
		widget.y = static_cast<int>(data.boundingBox.y);
		widget.w = static_cast<int>(data.boundingBox.width);
		widget.h = static_cast<int>(data.boundingBox.height);
	}
	RefreshElementState();
}

void UiAutomationRegistry::RefreshElementState() {
	elements_.clear();
	for(const auto& widget : widgets_){
		elements_.push_back(MetadataFromWidget(widget, MatchesFocus(widget)));
	}
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
bool FocusWidgetById(const std::string& id) { return ActiveUiAutomationRegistry().FocusWidgetById(id); }
bool IsTextInputFocused(int uid) { return ActiveUiAutomationRegistry().IsTextInputFocused(uid); }
bool HasFocus() { return ActiveUiAutomationRegistry().HasFocus(); }
void ClearFocus() { ActiveUiAutomationRegistry().ClearFocus(); }
bool DispatchTextInput(char ascii) { return ActiveUiAutomationRegistry().DispatchTextInput(ascii); }
bool BackspaceFocusedText() { return ActiveUiAutomationRegistry().BackspaceFocusedText(); }
bool SubmitFocusedText() { return ActiveUiAutomationRegistry().SubmitFocusedText(); }
bool CancelFocused() { return ActiveUiAutomationRegistry().CancelFocused(); }
bool InvokeAt(int x, int y) { return ActiveUiAutomationRegistry().InvokeAt(x, y); }
bool FocusNextInteractive() { return ActiveUiAutomationRegistry().FocusNextInteractive(); }
bool FocusPreviousInteractive() { return ActiveUiAutomationRegistry().FocusPreviousInteractive(); }
bool FocusDirectional(UiNavAction action) { return ActiveUiAutomationRegistry().FocusDirectional(action); }
bool ActivateFocused() { return ActiveUiAutomationRegistry().ActivateFocused(); }
void QueueAction(UiAction action) { ActiveUiAutomationRegistry().QueueAction(std::move(action)); }
void QueueClick(std::string id, void (*)(void *), void *) {
	UiAction action;
	action.kind = UiActionKind::Activate;
	action.id = std::move(id);
	action.value = action.id;
	ActiveUiAutomationRegistry().QueueAction(std::move(action));
}
void QueueRowSelect(std::string id, int rowIndex, void (*)(void *, int), void *) {
	UiAction action;
	action.kind = UiActionKind::Select;
	action.id = std::move(id);
	action.value = std::to_string(rowIndex);
	action.index = rowIndex;
	ActiveUiAutomationRegistry().QueueAction(std::move(action));
}
void QueueTextEnter(std::string id, const char * value, void (*)(void *), void *) {
	UiAction action;
	action.kind = UiActionKind::SubmitText;
	action.id = std::move(id);
	action.value = value ? value : "";
	ActiveUiAutomationRegistry().QueueAction(std::move(action));
}
std::vector<UiAction> DrainActions() { return ActiveUiAutomationRegistry().DrainActions(); }

}  // namespace automation

void DispatchUiActions(UiAutomationRegistry& registry, const std::vector<UiAction>& actions) {
	registry.DispatchActions(actions);
}

void DispatchUiActions(const std::vector<UiAction>& actions) {
	ActiveUiAutomationRegistry().DispatchActions(actions);
}

}  // namespace ui
}  // namespace silencer
