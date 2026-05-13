#pragma once

#include <string>
#include <vector>

namespace silencer {
namespace ui {

enum class UiNavAction {
	FocusNext,
	FocusPrevious,
	Up,
	Down,
	Left,
	Right,
	Confirm,
	Cancel,
	NextSection,
	PreviousSection,
};

struct UiPointerState {
	float x = 0.0f;
	float y = 0.0f;
	bool down = false;
	bool pressed = false;
	bool released = false;
	float wheelX = 0.0f;
	float wheelY = 0.0f;
};

struct UiInputState {
	int width = 0;
	int height = 0;
	float deltaTimeSeconds = 0.0f;
	UiPointerState pointer;
	std::string textInput;
	std::vector<UiNavAction> navActions;

	bool HasWindow() const { return width > 0 && height > 0; }
};

}  // namespace ui
}  // namespace silencer
