#pragma once

#include <string>
#include <vector>

namespace silencer {
namespace ui {

enum class UiActionKind {
	None,
	Navigate,
	Activate,
	SetText,
	SubmitText,
	Select,
	Cancel,
	Scroll,
	CaptureBinding,
};

enum class UiBindingInputKind {
	KeyboardKeyDown,
	GamepadButtonDown,
	GamepadAxisMoved,
};

struct UiBindingInput {
	UiBindingInputKind kind = UiBindingInputKind::KeyboardKeyDown;
	int code = 0;
	int axisDir = 0;
};

struct UiAction {
	UiActionKind kind = UiActionKind::None;
	std::string id;
	std::string value;
	int index = -1;
	int amount = 0;
	UiBindingInput binding;
};

enum class UiAutomationCommandKind {
	Action,
	InvokeAt,
};

struct UiAutomationCommand {
	UiAutomationCommandKind kind = UiAutomationCommandKind::Action;
	UiAction action;
	int x = 0;
	int y = 0;
};

class UiActionQueue {
public:
	void Push(UiAction action) { actions_.push_back(action); }
	bool Empty() const { return actions_.empty(); }
	const std::vector<UiAction>& Pending() const { return actions_; }

	std::vector<UiAction> Drain() {
		std::vector<UiAction> out;
		out.swap(actions_);
		return out;
	}

private:
	std::vector<UiAction> actions_;
};

}  // namespace ui
}  // namespace silencer
