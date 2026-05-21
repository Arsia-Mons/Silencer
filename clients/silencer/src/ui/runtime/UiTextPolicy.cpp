#include "runtime/UiTextPolicy.h"

namespace silencer {
namespace ui {

bool UiTextAllowsChar(char ascii, const UiTextPolicy& policy) {
	if(policy.numbersOnly && (ascii < '0' || ascii > '9')) return false;
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
	return true;
}

bool UiTextAppend(std::string& text, char ascii, const UiTextPolicy& policy) {
	if(!UiTextAllowsChar(ascii, policy)) return false;
	if(policy.maxLength > 0 && static_cast<int>(text.size()) >= policy.maxLength) return true;
	text.push_back(ascii);
	return true;
}

bool UiTextBackspace(std::string& text) {
	if(text.empty()) return false;
	text.pop_back();
	return true;
}

}  // namespace ui
}  // namespace silencer
