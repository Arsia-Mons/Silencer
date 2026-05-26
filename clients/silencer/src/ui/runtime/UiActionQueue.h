#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <utility>
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

enum class UiControlCommandKind {
	Action,
	PointerPress,
	PointerHover,
};

struct UiControlCommand {
	UiControlCommandKind kind = UiControlCommandKind::Action;
	UiAction action;
	int x = 0;
	int y = 0;
};

constexpr int UI_ACTION_QUEUE_MAX_ACTIONS = 128;

class UiActionQueue {
public:
	bool Push(UiAction action) {
		if(count_ >= UI_ACTION_QUEUE_MAX_ACTIONS) {
			++overflowCount_;
			return false;
		}
		actions_[count_++] = std::move(action);
		return true;
	}

	bool Empty() const { return count_ == 0; }
	int Count() const { return count_; }
	int OverflowCount() const { return overflowCount_; }

	std::vector<UiAction> Drain() {
		std::vector<UiAction> out;
		out.reserve(static_cast<std::size_t>(count_));
		for(int i = 0; i < count_; ++i){
			out.push_back(std::move(actions_[i]));
			actions_[i] = {};
		}
		count_ = 0;
		return out;
	}

private:
	std::array<UiAction, UI_ACTION_QUEUE_MAX_ACTIONS> actions_;
	int count_ = 0;
	int overflowCount_ = 0;
};

}  // namespace ui
}  // namespace silencer
