#pragma once

#include <string>
#include <vector>

#include "runtime/UiActionQueue.h"

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
	Backspace,
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
	// width/height are the virtual retained-layout dimensions (the native
	// surface size divided by uiScale). uiScale is the magnification used
	// when retained draw commands are rendered into the native surface.
	int width = 0;
	int height = 0;
	float uiScale = 1.0f;
	float deltaTimeSeconds = 0.0f;
	float animationDeltaSeconds = 1.0f / 24.0f;
	float animationStepSeconds = 1.0f / 24.0f;
	UiPointerState pointer;
	std::string textInput;
	std::vector<UiNavAction> navActions;
	std::vector<UiBindingInput> bindingInputs;
	std::vector<UiControlCommand> controlCommands;

	bool HasWindow() const { return width > 0 && height > 0; }
};

}  // namespace ui
}  // namespace silencer
