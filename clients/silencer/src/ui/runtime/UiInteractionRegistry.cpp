#include "runtime/UiInteractionRegistry.h"

#include "runtime/UiTextPolicy.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <utility>

namespace silencer {
namespace ui {

namespace registry_detail {

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

std::string InteractableId(const UiInteractable& widget) {
	if(!widget.id.empty()) return widget.id;
	if(widget.uid >= 0) return std::to_string(widget.uid);
	const char * label = UiInteractableLabel(widget);
	return label ? std::string(label) : std::string();
}

bool PointIn(const UiInteractable& widget, int x, int y) {
	return x >= widget.x && y >= widget.y
	    && x < widget.x + widget.w && y < widget.y + widget.h;
}

bool HasBounds(const UiInteractable& widget) {
	return widget.w > 0 && widget.h > 0;
}

UiElementKind MetadataKind(UiInteractableKind kind) {
	switch(kind){
		case UiInteractableKind::Button: return UiElementKind::Button;
		case UiInteractableKind::Toggle: return UiElementKind::Button;
		case UiInteractableKind::TextInput: return UiElementKind::TextField;
		case UiInteractableKind::ListRow: return UiElementKind::ListItem;
	}
	return UiElementKind::Container;
}

UiElementSnapshot MetadataFromWidget(const UiInteractable& widget, bool focused) {
	UiElementSnapshot metadata;
	metadata.id = InteractableId(widget);
	metadata.kind = MetadataKind(widget.kind);
	const char * label = UiInteractableLabel(widget);
	if(label) metadata.label = label;
	if(widget.kind == UiInteractableKind::TextInput){
		metadata.value = widget.isPassword
			? std::string(widget.value.size(), '*')
			: widget.value;
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

float CenterX(const UiInteractable& widget) {
	return static_cast<float>(widget.x) + static_cast<float>(widget.w) * 0.5f;
}

float CenterY(const UiInteractable& widget) {
	return static_cast<float>(widget.y) + static_cast<float>(widget.h) * 0.5f;
}

bool IsDirectionalCandidate(const UiInteractable& from,
                            const UiInteractable& to,
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

}  // namespace registry_detail

const char * UiInteractableLabel(const UiInteractable& widget) {
	return widget.labelText.empty() ? nullptr : widget.labelText.c_str();
}

bool UiInteractableMatchesLabel(const UiInteractable& widget, const char * label) {
	return registry_detail::LabelEquals(UiInteractableLabel(widget), label);
}

bool UiInteractableIsInteractive(const UiInteractable& widget) {
	if(widget.inactive) return false;
	return widget.kind == UiInteractableKind::Button ||
	       widget.kind == UiInteractableKind::Toggle ||
	       widget.kind == UiInteractableKind::ListRow ||
	       widget.kind == UiInteractableKind::TextInput;
}

void UiInteractionRegistry::BeginFrame() {
	elements_.clear();
	registeredElements_.clear();
	interactables_.clear();
}

void UiInteractionRegistry::Register(UiElementSnapshot metadata) {
	registeredElements_.push_back(std::move(metadata));
	RefreshElementState();
}

void UiInteractionRegistry::RegisterInteractable(UiInteractable widget) {
	const std::string incomingId = registry_detail::InteractableId(widget);
	UiInteractable * existing = nullptr;
	if(!incomingId.empty()){
		for(auto& candidate : interactables_){
			if(registry_detail::InteractableId(candidate) == incomingId){
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
		if(registry_detail::HasBounds(widget)){
			existing->x = widget.x;
			existing->y = widget.y;
			existing->w = widget.w;
			existing->h = widget.h;
		}
		if(!widget.id.empty()) existing->id = widget.id;
		if(!widget.labelText.empty()) existing->labelText = widget.labelText;
		existing->kind = widget.kind;
		if(widget.uid >= 0) existing->uid = widget.uid;
		if(widget.index >= 0) existing->index = widget.index;
		existing->selected = widget.selected;
		existing->value = widget.value;
		existing->maxLength = widget.maxLength;
		existing->isPassword = widget.isPassword;
		existing->inactive = widget.inactive;
		existing->numbersOnly = widget.numbersOnly;
		existing->cancelOnEscape = widget.cancelOnEscape;
	}else{
		if(widget.id.empty()) widget.id = incomingId;
		interactables_.push_back(std::move(widget));
	}

	RefreshElementState();
}

const std::vector<UiElementSnapshot>& UiInteractionRegistry::Elements() const {
	return elements_;
}

const std::vector<UiInteractable>& UiInteractionRegistry::Interactables() const {
	return interactables_;
}

const UiElementSnapshot* UiInteractionRegistry::FindById(const std::string& id) const {
	auto it = std::find_if(elements_.begin(), elements_.end(), [&](const UiElementSnapshot& element) {
		return element.id == id;
	});
	return it == elements_.end() ? nullptr : &*it;
}

const UiElementSnapshot* UiInteractionRegistry::FindByLabel(const std::string& label) const {
	auto it = std::find_if(elements_.begin(), elements_.end(), [&](const UiElementSnapshot& element) {
		return registry_detail::EqualsIgnoreCase(element.label, label);
	});
	return it == elements_.end() ? nullptr : &*it;
}

const UiInteractable* UiInteractionRegistry::FindInteractableByLabel(const char * label) const {
	if(!label || !*label) return nullptr;
	const UiInteractable * hit = nullptr;
	int count = 0;
	for(const auto& widget : interactables_){
		if(UiInteractableMatchesLabel(widget, label)){
			hit = &widget;
			++count;
		}
	}
	return count == 1 ? hit : nullptr;
}

const UiInteractable* UiInteractionRegistry::FindInteractableByUid(int uid) const {
	const UiInteractable * hit = nullptr;
	int count = 0;
	for(const auto& widget : interactables_){
		if(widget.uid == uid){
			hit = &widget;
			++count;
		}
	}
	return count == 1 ? hit : nullptr;
}

const UiInteractable* UiInteractionRegistry::FindInteractableById(const std::string& id) const {
	for(const auto& widget : interactables_){
		if(registry_detail::InteractableId(widget) == id) return &widget;
	}
	return nullptr;
}

bool UiInteractionRegistry::MatchesFocus(const UiInteractable& widget) const {
	if(focusedUid_ >= 0 && widget.uid == focusedUid_) return true;
	if(!focusedLabel_.empty() && registry_detail::InteractableId(widget) == focusedLabel_) return true;
	const char * label = UiInteractableLabel(widget);
	return focusedUid_ < 0 && !focusedLabel_.empty() && label
	    && registry_detail::LabelEquals(label, focusedLabel_.c_str());
}

const UiInteractable* UiInteractionRegistry::FocusedInteractable() const {
	for(const auto& widget : interactables_){
		if(MatchesFocus(widget)) return &widget;
	}
	return nullptr;
}

UiInteractable* UiInteractionRegistry::FocusedInteractable() {
	for(auto& widget : interactables_){
		if(MatchesFocus(widget)) return &widget;
	}
	return nullptr;
}

void UiInteractionRegistry::SetFocus(const UiInteractable& widget,
                                     FocusOrigin origin) {
	focusedUid_ = widget.uid;
	focusedKind_ = widget.kind;
	focusedLabel_ = registry_detail::InteractableId(widget);
	focusedOrigin_ = origin;
	RefreshElementState();
}

bool UiInteractionRegistry::QueueAction(UiActionKind kind,
                                       const UiInteractable& widget,
                                       const char * value) {
	UiAction action;
	action.kind = kind;
	action.id = registry_detail::InteractableId(widget);
	if(value) action.value = value;
	else{
		const char * label = UiInteractableLabel(widget);
		if(label) action.value = label;
	}
	action.index = widget.index;
	return actions_.Push(std::move(action));
}

bool UiInteractionRegistry::FocusTextInputAt(int x, int y) {
	for(auto it = interactables_.rbegin(); it != interactables_.rend(); ++it){
		if(it->kind == UiInteractableKind::TextInput &&
		   !it->inactive && registry_detail::PointIn(*it, x, y)){
			SetFocus(*it, FocusOrigin::Text);
			QueueAction(UiActionKind::Select, *it, it->value.c_str());
			return true;
		}
	}
	return false;
}

bool UiInteractionRegistry::FocusTextInputByUid(int uid) {
	for(const auto& widget : interactables_){
		if(widget.kind == UiInteractableKind::TextInput &&
		   widget.uid == uid && !widget.inactive){
			SetFocus(widget, FocusOrigin::Text);
			QueueAction(UiActionKind::Select, widget, widget.value.c_str());
			return true;
		}
	}
	return false;
}

bool UiInteractionRegistry::FocusInteractableById(const std::string& id) {
	const UiInteractable * widget = FindInteractableById(id);
	if(!widget || !UiInteractableIsInteractive(*widget)) return false;
	SetFocus(*widget, widget->kind == UiInteractableKind::TextInput
	                  ? FocusOrigin::Text
	                  : FocusOrigin::Navigation);
	return true;
}

bool UiInteractionRegistry::IsTextInputFocused(int uid) const {
	const UiInteractable * widget = FocusedInteractable();
	if(widget) return widget->kind == UiInteractableKind::TextInput && widget->uid == uid;
	return focusedKind_ == UiInteractableKind::TextInput && focusedUid_ == uid;
}

bool UiInteractionRegistry::HasFocus() const {
	return FocusedInteractable() != nullptr;
}

bool UiInteractionRegistry::HasTextInputFocus() const {
	const UiInteractable * w = FocusedInteractable();
	return w && w->kind == UiInteractableKind::TextInput && !w->inactive;
}

void UiInteractionRegistry::ClearFocus() {
	focusedUid_ = -1;
	focusedKind_ = UiInteractableKind::Button;
	focusedLabel_.clear();
	focusedOrigin_ = FocusOrigin::None;
	RefreshElementState();
}

bool UiInteractionRegistry::DispatchTextInput(char ascii) {
	UiInteractable * widget = FocusedInteractable();
	if(!widget || widget->kind != UiInteractableKind::TextInput || widget->inactive) return false;
	UiTextPolicy policy;
	policy.numbersOnly = widget->numbersOnly;
	policy.maxLength = widget->maxLength;
	if(!UiTextAllowsChar(ascii, policy)) return false;
	if(!UiTextAppend(widget->value, ascii, policy)) return false;
	QueueAction(UiActionKind::SetText, *widget, widget->value.c_str());
	RefreshElementState();
	return true;
}

bool UiInteractionRegistry::BackspaceFocusedText() {
	UiInteractable * widget = FocusedInteractable();
	if(!widget || widget->kind != UiInteractableKind::TextInput || widget->inactive){
		return false;
	}
	UiTextBackspace(widget->value);
	QueueAction(UiActionKind::SetText, *widget, widget->value.c_str());
	RefreshElementState();
	return true;
}

bool UiInteractionRegistry::SubmitFocusedText() {
	const UiInteractable * widget = FocusedInteractable();
	if(widget && widget->kind == UiInteractableKind::TextInput && !widget->inactive){
		QueueAction(UiActionKind::SubmitText, *widget, widget->value.c_str());
		return true;
	}
	return false;
}

bool UiInteractionRegistry::CancelFocused() {
	const UiInteractable * widget = FocusedInteractable();
	if(widget && widget->kind == UiInteractableKind::TextInput && !widget->inactive){
		if(widget->cancelOnEscape) QueueAction(UiActionKind::Cancel, *widget, widget->value.c_str());
		ClearFocus();
		return true;
	}
	return false;
}

bool UiInteractionRegistry::PressAt(int x, int y) {
	for(auto it = interactables_.rbegin(); it != interactables_.rend(); ++it){
		if(UiInteractableIsInteractive(*it) && registry_detail::PointIn(*it, x, y)){
			SetFocus(*it, it->kind == UiInteractableKind::TextInput
			              ? FocusOrigin::Text
			              : FocusOrigin::Pointer);
			switch(it->kind){
				case UiInteractableKind::Button:
				case UiInteractableKind::Toggle:
					QueueAction(UiActionKind::Activate, *it, nullptr);
					return true;
				case UiInteractableKind::ListRow:
					QueueAction(UiActionKind::Select, *it, "pointer");
					return true;
				case UiInteractableKind::TextInput:
					QueueAction(UiActionKind::Select, *it, it->value.c_str());
					return true;
			}
		}
	}
	ClearFocus();
	return false;
}

bool UiInteractionRegistry::FocusNextInteractive() {
	std::vector<const UiInteractable *> items;
	for(const auto& widget : interactables_){
		if(UiInteractableIsInteractive(widget)) items.push_back(&widget);
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
	SetFocus(*items[next], FocusOrigin::Navigation);
	QueueAction(UiActionKind::Navigate, *items[next], "focus_next");
	return true;
}

bool UiInteractionRegistry::FocusPreviousInteractive() {
	std::vector<const UiInteractable *> items;
	for(const auto& widget : interactables_){
		if(UiInteractableIsInteractive(widget)) items.push_back(&widget);
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
	SetFocus(*items[next], FocusOrigin::Navigation);
	QueueAction(UiActionKind::Navigate, *items[next], "focus_previous");
	return true;
}

bool UiInteractionRegistry::FocusDirectional(UiNavAction action) {
	const UiInteractable * focused = FocusedInteractable();
	if(!focused || !registry_detail::HasBounds(*focused)) {
		if(action == UiNavAction::Down || action == UiNavAction::Right) return FocusNextInteractive();
		if(action == UiNavAction::Up || action == UiNavAction::Left) return FocusPreviousInteractive();
		return false;
	}

	const UiInteractable * best = nullptr;
	float bestPrimary = 0.0f;
	float bestSecondary = 0.0f;
	for(const auto& widget : interactables_){
		if(&widget == focused || !UiInteractableIsInteractive(widget) ||
		   !registry_detail::HasBounds(widget)) continue;
		float primary = 0.0f;
		float secondary = 0.0f;
		if(!registry_detail::IsDirectionalCandidate(*focused, widget, action, primary, secondary)) continue;
		if(!best || primary < bestPrimary ||
		   (primary == bestPrimary && secondary < bestSecondary)){
			best = &widget;
			bestPrimary = primary;
			bestSecondary = secondary;
		}
	}
	if(!best) return false;
	SetFocus(*best, FocusOrigin::Navigation);
	switch(action){
		case UiNavAction::Up: QueueAction(UiActionKind::Navigate, *best, "up"); break;
		case UiNavAction::Down: QueueAction(UiActionKind::Navigate, *best, "down"); break;
		case UiNavAction::Left: QueueAction(UiActionKind::Navigate, *best, "left"); break;
		case UiNavAction::Right: QueueAction(UiActionKind::Navigate, *best, "right"); break;
		default: break;
	}
	return true;
}

bool UiInteractionRegistry::FocusHovered(float x, float y) {
	return FocusHoveredAt(x, y, true);
}

bool UiInteractionRegistry::FocusControlHovered(float x, float y) {
	return FocusHoveredAt(x, y, false);
}

bool UiInteractionRegistry::FocusHoveredAt(float x, float y, bool recordPhysicalSample) {
	// Only react to actual pointer movement. A resting pointer that happens to
	// sit over a row must not keep yanking focus back from the keyboard.
	if(recordPhysicalSample && haveHoverSample_ && x == hoverSampleX_ && y == hoverSampleY_){
		return false;
	}
	if(recordPhysicalSample){
		haveHoverSample_ = true;
		hoverSampleX_ = x;
		hoverSampleY_ = y;
	}

	const UiInteractable * focused = FocusedInteractable();
	if(focused && focused->kind == UiInteractableKind::TextInput && !focused->inactive){
		return false;
	}

	const int ix = static_cast<int>(x);
	const int iy = static_cast<int>(y);
	for(auto it = interactables_.rbegin(); it != interactables_.rend(); ++it){
		if(it->kind == UiInteractableKind::TextInput) continue;
		if(!UiInteractableIsInteractive(*it)) continue;
		if(!registry_detail::PointIn(*it, ix, iy)) continue;
		if(MatchesFocus(*it)){
			if(focusedOrigin_ != FocusOrigin::Pointer){
				SetFocus(*it, FocusOrigin::Pointer);
			}
			return true;
		}
		SetFocus(*it, FocusOrigin::Pointer);
		QueueAction(UiActionKind::Navigate, *it, "hover");
		return true;
	}
	if(focusedOrigin_ == FocusOrigin::Pointer &&
	   (focusedUid_ >= 0 || !focusedLabel_.empty())){
		ClearFocus();
		return true;
	}
	// Over empty space or a text field: leave keyboard/gamepad/text focus in place.
	return false;
}

bool UiInteractionRegistry::ActivateFocused() {
	const UiInteractable * widget = FocusedInteractable();
	if(!widget || widget->inactive) return false;
	switch(widget->kind){
		case UiInteractableKind::Button:
		case UiInteractableKind::Toggle:
			QueueAction(UiActionKind::Activate, *widget, nullptr);
			return true;
		case UiInteractableKind::ListRow:
			QueueAction(UiActionKind::Select, *widget, "activate");
			return true;
		case UiInteractableKind::TextInput:
			QueueAction(UiActionKind::Select, *widget, widget->value.c_str());
			return true;
	}
	return false;
}

bool UiInteractionRegistry::QueueAction(UiAction action) {
	return actions_.Push(std::move(action));
}

std::vector<UiAction> UiInteractionRegistry::DrainActions() {
	return actions_.Drain();
}

void UiInteractionRegistry::ResolveClayBoundsFromClay() {
	for(auto& element : registeredElements_){
		if(!element.hasClayId) continue;
		Clay_ElementData data = Clay_GetElementData(element.clayId);
		if(!data.found) continue;
		element.bounds = UiRect{
			data.boundingBox.x,
			data.boundingBox.y,
			data.boundingBox.width,
			data.boundingBox.height,
		};
	}
	for(auto& widget : interactables_){
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

void UiInteractionRegistry::RefreshElementState() {
	elements_ = registeredElements_;
	for(const auto& widget : interactables_){
		elements_.push_back(registry_detail::MetadataFromWidget(widget, MatchesFocus(widget)));
	}
}


}  // namespace ui
}  // namespace silencer
