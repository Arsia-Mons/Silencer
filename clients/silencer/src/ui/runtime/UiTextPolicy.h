#pragma once

#include <string>

namespace silencer {
namespace ui {

struct UiTextPolicy {
	bool numbersOnly = false;
	int maxLength = 0;
};

bool UiTextAllowsChar(char ascii, const UiTextPolicy& policy = {});
bool UiTextAppend(std::string& text, char ascii, const UiTextPolicy& policy = {});
bool UiTextBackspace(std::string& text);

}  // namespace ui
}  // namespace silencer
