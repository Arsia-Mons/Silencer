#pragma once

#include "runtime/UiActionQueue.h"
#include "runtime/UiAutomationRegistry.h"

namespace silencer {
namespace ui {

struct ButtonProps {
	const char* id = "";
	const char* label = "";
	UiRect bounds;
	bool enabled = true;
};

void RegisterButton(const ButtonProps& props, UiAutomationRegistry& automation);
void QueueButtonActivation(const ButtonProps& props, UiActionQueue& actions);

}  // namespace ui
}  // namespace silencer
