#include "runtime/UiInteractionRegistry.h"

#include "runtime/UiTextPolicy.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>

namespace silencer {
namespace ui {

namespace registry_detail {

struct InteractableIdentity {
	const char * text = nullptr;
	std::size_t len = 0;
	char uidText[16] = {};

	bool empty() const { return len == 0; }
};

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

bool TextEquals(const char * lhs, std::size_t lhsLen, const char * rhs, std::size_t rhsLen) {
	if(lhsLen != rhsLen) return false;
	if(lhsLen == 0) return true;
	if(!lhs || !rhs) return false;
	return std::memcmp(lhs, rhs, lhsLen) == 0;
}

void FillInteractableIdentity(InteractableIdentity& identity, const UiInteractable& widget) {
	identity = {};
	if(!widget.id.empty()){
		identity.text = widget.id.data();
		identity.len = widget.id.size();
		return;
	}
	if(widget.uid >= 0){
		const int n = std::snprintf(identity.uidText, sizeof(identity.uidText), "%d", widget.uid);
		if(n > 0){
			identity.text = identity.uidText;
			identity.len = n < static_cast<int>(sizeof(identity.uidText))
				? static_cast<std::size_t>(n)
				: sizeof(identity.uidText) - 1;
		}
		return;
	}
	const char * label = UiInteractableLabel(widget);
	if(label){
		identity.text = label;
		identity.len = std::strlen(label);
	}
}

bool InteractableMatchesText(const UiInteractable& widget, const char * text, std::size_t len) {
	InteractableIdentity identity;
	FillInteractableIdentity(identity, widget);
	return TextEquals(identity.text, identity.len, text, len);
}

bool InteractableMatchesIdentity(const UiInteractable& widget,
                                 const InteractableIdentity& identity) {
	if(identity.empty()) return false;
	return InteractableMatchesText(widget, identity.text, identity.len);
}

void AssignStringFromIdentity(std::string& out, const InteractableIdentity& identity) {
	if(identity.text){
		out.assign(identity.text, identity.len);
	}else{
		out.clear();
	}
}

void AssignInteractableActionId(UiActionId& id, const UiInteractable& widget) {
	InteractableIdentity identity;
	FillInteractableIdentity(identity, widget);
	id.Assign(identity.text, identity.len);
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
	InteractableIdentity identity;
	FillInteractableIdentity(identity, widget);
	AssignStringFromIdentity(metadata.id, identity);
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
	for(int i = 0; i < elementCount_; ++i){
		elements_[i] = {};
	}
	for(int i = 0; i < registeredElementCount_; ++i){
		registeredElements_[i] = {};
	}
	for(int i = 0; i < interactableCount_; ++i){
		interactables_[i] = {};
	}
	elementCount_ = 0;
	registeredElementCount_ = 0;
	interactableCount_ = 0;
}

bool UiInteractionRegistry::Register(UiElementSnapshot metadata) {
	if(registeredElementCount_ >= UI_INTERACTION_MAX_REGISTERED_ELEMENTS) {
		++elementOverflowCount_;
		return false;
	}
	registeredElements_[registeredElementCount_++] = std::move(metadata);
	RefreshElementState();
	return true;
}

bool UiInteractionRegistry::RegisterInteractable(UiInteractable widget) {
	registry_detail::InteractableIdentity incomingId;
	registry_detail::FillInteractableIdentity(incomingId, widget);
	UiInteractable * existing = nullptr;
	if(!incomingId.empty()){
		for(int i = 0; i < interactableCount_; ++i){
			UiInteractable& candidate = interactables_[i];
			if(registry_detail::InteractableMatchesIdentity(candidate, incomingId)){
				existing = &candidate;
				break;
			}
		}
	}

	UiInteractable * registered = existing;
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
		existing->requestInitialFocus = widget.requestInitialFocus;
		existing->requestFocus = widget.requestFocus;
	}else{
		if(interactableCount_ >= UI_INTERACTION_MAX_INTERACTABLES) {
			++interactableOverflowCount_;
			return false;
		}
		interactables_[interactableCount_++] = std::move(widget);
		registered = &interactables_[interactableCount_ - 1];
	}

	if(registered) RegisterFocusRuntimeTarget(*registered);
	RefreshElementState();
	return true;
}

UiElementSnapshotSpan UiInteractionRegistry::Elements() const {
	return UiElementSnapshotSpan(elements_.data(), elementCount_);
}

UiInteractableSpan UiInteractionRegistry::Interactables() const {
	return UiInteractableSpan(interactables_.data(), interactableCount_);
}

const UiElementSnapshot* UiInteractionRegistry::FindById(const std::string& id) const {
	for(int i = 0; i < elementCount_; ++i){
		if(elements_[i].id == id) return &elements_[i];
	}
	return nullptr;
}

const UiElementSnapshot* UiInteractionRegistry::FindByLabel(const std::string& label) const {
	for(int i = 0; i < elementCount_; ++i){
		if(registry_detail::EqualsIgnoreCase(elements_[i].label, label)) return &elements_[i];
	}
	return nullptr;
}

const UiInteractable* UiInteractionRegistry::FindInteractableByLabel(const char * label) const {
	if(!label || !*label) return nullptr;
	const UiInteractable * hit = nullptr;
	int count = 0;
	for(int i = 0; i < interactableCount_; ++i){
		const UiInteractable& widget = interactables_[i];
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
	for(int i = 0; i < interactableCount_; ++i){
		const UiInteractable& widget = interactables_[i];
		if(widget.uid == uid){
			hit = &widget;
			++count;
		}
	}
	return count == 1 ? hit : nullptr;
}

const UiInteractable* UiInteractionRegistry::FindInteractableById(const char * id) const {
	return FindInteractableById(id, id ? std::strlen(id) : 0);
}

const UiInteractable* UiInteractionRegistry::FindInteractableById(const char * id,
                                                                  std::size_t len) const {
	for(int i = 0; i < interactableCount_; ++i){
		if(registry_detail::InteractableMatchesText(interactables_[i], id, len)) {
			return &interactables_[i];
		}
	}
	return nullptr;
}

const UiElementSnapshot* UiInteractionRegistry::FindElementForInteractable(
	const UiInteractable& widget) const {
	registry_detail::InteractableIdentity identity;
	registry_detail::FillInteractableIdentity(identity, widget);
	if(identity.empty()) return nullptr;
	for(int i = elementCount_ - 1; i >= 0; --i){
		const UiElementSnapshot& element = elements_[i];
		if(registry_detail::TextEquals(element.id.data(), element.id.size(),
		                               identity.text, identity.len)){
			return &element;
		}
	}
	return nullptr;
}

bool UiInteractionRegistry::MatchesFocus(const UiInteractable& widget) const {
	if(focusedUid_ >= 0 && widget.uid == focusedUid_) return true;
	if(!focusedLabel_.empty() &&
	   registry_detail::InteractableMatchesText(widget, focusedLabel_.data(), focusedLabel_.size())){
		return true;
	}
	const char * label = UiInteractableLabel(widget);
	return focusedUid_ < 0 && !focusedLabel_.empty() && label
	    && registry_detail::LabelEquals(label, focusedLabel_.c_str());
}

const UiInteractable* UiInteractionRegistry::FocusedInteractable() const {
	for(int i = 0; i < interactableCount_; ++i){
		if(MatchesFocus(interactables_[i])) return &interactables_[i];
	}
	return nullptr;
}

UiInteractable* UiInteractionRegistry::FocusedInteractable() {
	for(int i = 0; i < interactableCount_; ++i){
		if(MatchesFocus(interactables_[i])) return &interactables_[i];
	}
	return nullptr;
}

void UiInteractionRegistry::SetFocus(const UiInteractable& widget,
                                     FocusOrigin origin) {
	focusedUid_ = widget.uid;
	focusedKind_ = widget.kind;
	registry_detail::AssignInteractableActionId(focusedLabel_, widget);
	focusedOrigin_ = origin;
	RefreshElementState();
}

void UiInteractionRegistry::RegisterFocusRuntimeTarget(const UiInteractable& widget) {
	if(!widget.hasClayId || widget.clayId.id == 0) return;

	registry_detail::InteractableIdentity identity;
	registry_detail::FillInteractableIdentity(identity, widget);
	if(identity.empty()) return;

	std::string id;
	registry_detail::AssignStringFromIdentity(id, identity);
	if(widget.requestFocus){
		ui_focus_request_focus(widget.clayId);
	}else if(widget.requestInitialFocus){
		ui_focus_request_initial_focus(widget.clayId);
	}
	UiFocusableState state = ui_focusable({
		widget.clayId,
		widget.inactive,
		{},
		[this, id] {
			ConfirmFromFocusRuntime(id.data(), id.size());
		},
		[this, id] {
			FocusFromFocusRuntime(id.data(), id.size());
		},
	});

	if(state.focused){
		FocusOrigin origin = FocusOrigin::Navigation;
		UiFocusSource source = ui_focus_source();
		if(widget.kind == UiInteractableKind::TextInput){
			origin = FocusOrigin::Text;
		}else if(source == UiFocusSource::Mouse || source == UiFocusSource::Touch){
			origin = FocusOrigin::Pointer;
		}
		SetFocus(widget, origin);
	}
}

void UiInteractionRegistry::FocusFromFocusRuntime(const char * id, std::size_t len) {
	const UiInteractable * widget = FindInteractableById(id, len);
	if(!widget || !UiInteractableIsInteractive(*widget)) return;

	FocusOrigin origin = FocusOrigin::Navigation;
	UiFocusSource source = ui_focus_source();
	if(widget->kind == UiInteractableKind::TextInput){
		origin = FocusOrigin::Text;
	}else if(source == UiFocusSource::Mouse || source == UiFocusSource::Touch){
		origin = FocusOrigin::Pointer;
	}
	SetFocus(*widget, origin);
	if(widget->kind == UiInteractableKind::TextInput){
		QueueAction(UiActionKind::Select, *widget, widget->value.c_str());
	}else{
		QueueAction(UiActionKind::Navigate, *widget, "focus");
	}
}

void UiInteractionRegistry::ConfirmFromFocusRuntime(const char * id, std::size_t len) {
	const UiInteractable * widget = FindInteractableById(id, len);
	if(!widget || !UiInteractableIsInteractive(*widget)) return;

	SetFocus(*widget, widget->kind == UiInteractableKind::TextInput
	                  ? FocusOrigin::Text
	                  : FocusOrigin::Navigation);
	switch(widget->kind){
		case UiInteractableKind::Button:
		case UiInteractableKind::Toggle:
			QueueAction(UiActionKind::Activate, *widget, nullptr);
			return;
		case UiInteractableKind::ListRow:
			QueueAction(UiActionKind::Select, *widget, "activate");
			return;
		case UiInteractableKind::TextInput:
			return;
	}
}

bool UiInteractionRegistry::QueueAction(UiActionKind kind,
                                       const UiInteractable& widget,
                                       const char * value) {
	UiAction action;
	action.kind = kind;
	registry_detail::AssignInteractableActionId(action.id, widget);
	if(value) action.value = value;
	else{
		const char * label = UiInteractableLabel(widget);
		if(label) action.value = label;
	}
	action.index = widget.index;
	return actions_.Push(std::move(action));
}

bool UiInteractionRegistry::FocusTextInputAt(int x, int y) {
	for(int i = interactableCount_ - 1; i >= 0; --i){
		UiInteractable& widget = interactables_[i];
		if(widget.kind == UiInteractableKind::TextInput &&
		   !widget.inactive && registry_detail::PointIn(widget, x, y)){
			SetFocus(widget, FocusOrigin::Text);
			QueueAction(UiActionKind::Select, widget, widget.value.c_str());
			return true;
		}
	}
	return false;
}

bool UiInteractionRegistry::FocusTextInputByUid(int uid) {
	for(int i = 0; i < interactableCount_; ++i){
		const UiInteractable& widget = interactables_[i];
		if(widget.kind == UiInteractableKind::TextInput &&
		   widget.uid == uid && !widget.inactive){
			SetFocus(widget, FocusOrigin::Text);
			QueueAction(UiActionKind::Select, widget, widget.value.c_str());
			return true;
		}
	}
	return false;
}

void UiInteractionRegistry::RequestTextInputFocusByUid(int uid) {
	if(uid < 0) return;
	focusedUid_ = uid;
	focusedKind_ = UiInteractableKind::TextInput;
	focusedLabel_.clear();
	focusedOrigin_ = FocusOrigin::Text;
	RefreshElementState();
}

bool UiInteractionRegistry::FocusInteractableById(const char * id) {
	return FocusInteractableById(id, id ? std::strlen(id) : 0);
}

bool UiInteractionRegistry::FocusInteractableById(const char * id, std::size_t len) {
	const UiInteractable * widget = FindInteractableById(id, len);
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
	for(int i = interactableCount_ - 1; i >= 0; --i){
		UiInteractable& widget = interactables_[i];
		if(UiInteractableIsInteractive(widget) && registry_detail::PointIn(widget, x, y)){
			SetFocus(widget, widget.kind == UiInteractableKind::TextInput
			              ? FocusOrigin::Text
			              : FocusOrigin::Pointer);
			switch(widget.kind){
				case UiInteractableKind::Button:
				case UiInteractableKind::Toggle:
					QueueAction(UiActionKind::Activate, widget, nullptr);
					return true;
				case UiInteractableKind::ListRow:
					QueueAction(UiActionKind::Select, widget, "pointer");
					return true;
				case UiInteractableKind::TextInput:
					QueueAction(UiActionKind::Select, widget, widget.value.c_str());
					return true;
			}
		}
	}
	ClearFocus();
	return false;
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
	for(int i = interactableCount_ - 1; i >= 0; --i){
		UiInteractable& widget = interactables_[i];
		if(widget.kind == UiInteractableKind::TextInput) continue;
		if(!UiInteractableIsInteractive(widget)) continue;
		if(!registry_detail::PointIn(widget, ix, iy)) continue;
		if(MatchesFocus(widget)){
			if(focusedOrigin_ != FocusOrigin::Pointer){
				SetFocus(widget, FocusOrigin::Pointer);
			}
			return true;
		}
		SetFocus(widget, FocusOrigin::Pointer);
		QueueAction(UiActionKind::Navigate, widget, "hover");
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

bool UiInteractionRegistry::QueueAction(UiAction action) {
	return actions_.Push(std::move(action));
}

UiActionList UiInteractionRegistry::DrainActions() {
	return actions_.Drain();
}

void UiInteractionRegistry::ResolveClayBoundsFromClay() {
	for(int i = 0; i < registeredElementCount_; ++i){
		UiElementSnapshot& element = registeredElements_[i];
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
	for(int i = 0; i < interactableCount_; ++i){
		UiInteractable& widget = interactables_[i];
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
	for(int i = 0; i < elementCount_; ++i){
		elements_[i] = {};
	}
	elementCount_ = 0;
	for(int i = 0; i < registeredElementCount_; ++i){
		elements_[elementCount_++] = registeredElements_[i];
	}
	for(int i = 0; i < interactableCount_; ++i){
		if(elementCount_ >= UI_INTERACTION_MAX_ELEMENTS) {
			++elementOverflowCount_;
			break;
		}
		const UiInteractable& widget = interactables_[i];
		elements_[elementCount_++] =
			registry_detail::MetadataFromWidget(widget, MatchesFocus(widget));
	}
}


}  // namespace ui
}  // namespace silencer
