#include "clay_inspector.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace silencer::ui::clay_inspector {

namespace {

std::vector<Widget> & Registry() {
	static std::vector<Widget> v;
	return v;
}

bool IEq(const char * a, const char * b) {
	if(!a || !b) return false;
	while(*a && *b){
		if(std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
		++a; ++b;
	}
	return *a == 0 && *b == 0;
}

struct FocusRef {
	int uid = -1;
	std::string label;
};

FocusRef & Focus() {
	static FocusRef f;
	return f;
}

bool MatchesFocus(const Widget & w) {
	const FocusRef & f = Focus();
	if(f.uid >= 0 && w.uid == f.uid) return true;
	return f.uid < 0 && !f.label.empty() && w.label && IEq(w.label, f.label.c_str());
}

bool PointIn(const Widget & w, int x, int y) {
	return x >= w.x && y >= w.y && x < w.x + w.w && y < w.y + w.h;
}

const Widget * FocusedWidget() {
	const auto & v = Registry();
	for(const auto & w : v){
		if(MatchesFocus(w)) return &w;
	}
	return nullptr;
}

void SetFocus(const Widget & w) {
	FocusRef & f = Focus();
	f.uid = w.uid;
	f.label = w.label ? w.label : "";
}

bool IsInteractive(const Widget & w) {
	if(w.inactive) return false;
	return w.kind == WidgetKind::Button ||
	       w.kind == WidgetKind::Toggle ||
	       w.kind == WidgetKind::ListRow ||
	       w.kind == WidgetKind::TextInput;
}

bool Invoke(const Widget & w) {
	if(w.inactive) return false;
	switch(w.kind){
		case WidgetKind::Button:
		case WidgetKind::Toggle:
			if(w.onClick){
				w.onClick(w.clickUser);
				return true;
			}
			return false;
		case WidgetKind::ListRow:
			if(w.onClickRow){
				w.onClickRow(w.clickUser, w.rowIndex);
				return true;
			}
			return false;
		case WidgetKind::TextInput:
			SetFocus(w);
			return true;
	}
	return false;
}

bool AppendChar(Widget const & w, char ascii) {
	if(!w.textBuffer || w.textBufferLen <= 0 || w.inactive) return false;
	if(w.numbersOnly && (ascii < '0' || ascii > '9')) return false;
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
	int len = static_cast<int>(std::strlen(w.textBuffer));
	if(len >= w.textBufferLen - 1) return true;
	w.textBuffer[len] = ascii;
	w.textBuffer[len + 1] = '\0';
	return true;
}

bool Backspace(Widget const & w) {
	if(!w.textBuffer || w.textBufferLen <= 0 || w.inactive) return false;
	int len = static_cast<int>(std::strlen(w.textBuffer));
	if(len > 0) w.textBuffer[len - 1] = '\0';
	return true;
}

bool FocusNext() {
	const auto & v = Registry();
	std::vector<const Widget *> inputs;
	for(const auto & w : v){
		if(w.kind == WidgetKind::TextInput && !w.inactive) inputs.push_back(&w);
	}
	if(inputs.empty()) return false;
	int current = -1;
	for(int i = 0; i < static_cast<int>(inputs.size()); i++){
		if(MatchesFocus(*inputs[i])){
			current = i;
			break;
		}
	}
	SetFocus(*inputs[(current + 1) % static_cast<int>(inputs.size())]);
	return true;
}

}  // namespace

void BeginFrame() { Registry().clear(); }

void Register(const Widget & w) { Registry().push_back(w); }

const std::vector<Widget> & All() { return Registry(); }

const Widget * FindByLabel(const char * label) {
	if(!label || !*label) return nullptr;
	const auto & v = Registry();
	const Widget * hit = nullptr;
	int count = 0;
	for(const auto & w : v){
		if(w.label && IEq(w.label, label)){
			hit = &w;
			++count;
		}
	}
	return count == 1 ? hit : nullptr;
}

const Widget * FindByUid(int uid) {
	const auto & v = Registry();
	const Widget * hit = nullptr;
	int count = 0;
	for(const auto & w : v){
		if(w.uid == uid){
			hit = &w;
			++count;
		}
	}
	return count == 1 ? hit : nullptr;
}

bool FocusTextInputAt(int x, int y) {
	const auto & v = Registry();
	for(auto it = v.rbegin(); it != v.rend(); ++it){
		if(it->kind == WidgetKind::TextInput && !it->inactive && PointIn(*it, x, y)){
			SetFocus(*it);
			return true;
		}
	}
	return false;
}

bool FocusTextInputByUid(int uid) {
	const auto & v = Registry();
	for(const auto & w : v){
		if(w.kind == WidgetKind::TextInput && w.uid == uid && !w.inactive){
			SetFocus(w);
			return true;
		}
	}
	return false;
}

bool IsTextInputFocused(int uid) {
	const Widget * w = FocusedWidget();
	return w && w->uid == uid;
}

void ClearFocus() {
	Focus().uid = -1;
	Focus().label.clear();
}

bool DispatchTextInput(char ascii) {
	const Widget * w = FocusedWidget();
	return w ? AppendChar(*w, ascii) : false;
}

bool DispatchKeyPress(char ascii) {
	const Widget * w = FocusedWidget();
	switch(ascii){
		case '\b':
			return (w && w->kind == WidgetKind::TextInput) ? Backspace(*w) : false;
		case '\t':
			return FocusNext();
		case '\n':
			if(w && w->kind == WidgetKind::TextInput && !w->inactive){
				if(w->onEnter) w->onEnter(w->enterUser);
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

bool InvokeAt(int x, int y) {
	const auto & v = Registry();
	for(auto it = v.rbegin(); it != v.rend(); ++it){
		if(IsInteractive(*it) && PointIn(*it, x, y)){
			SetFocus(*it);
			return Invoke(*it);
		}
	}
	ClearFocus();
	return false;
}

bool FocusNextInteractive() {
	const auto & v = Registry();
	std::vector<const Widget *> items;
	for(const auto & w : v){
		if(IsInteractive(w)) items.push_back(&w);
	}
	if(items.empty()) return false;
	int current = -1;
	for(int i = 0; i < static_cast<int>(items.size()); i++){
		if(MatchesFocus(*items[i])){
			current = i;
			break;
		}
	}
	SetFocus(*items[(current + 1) % static_cast<int>(items.size())]);
	return true;
}

bool FocusPreviousInteractive() {
	const auto & v = Registry();
	std::vector<const Widget *> items;
	for(const auto & w : v){
		if(IsInteractive(w)) items.push_back(&w);
	}
	if(items.empty()) return false;
	int current = 0;
	for(int i = 0; i < static_cast<int>(items.size()); i++){
		if(MatchesFocus(*items[i])){
			current = i;
			break;
		}
	}
	int next = current - 1;
	if(next < 0) next = static_cast<int>(items.size()) - 1;
	SetFocus(*items[next]);
	return true;
}

bool ActivateFocused() {
	const Widget * w = FocusedWidget();
	return w ? Invoke(*w) : false;
}

}  // namespace silencer::ui::clay_inspector
