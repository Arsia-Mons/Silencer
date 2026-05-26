#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <utility>

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

class UiActionList {
public:
	using iterator = UiAction *;
	using const_iterator = const UiAction *;

	bool Push(UiAction action) {
		if(count_ >= UI_ACTION_QUEUE_MAX_ACTIONS) {
			++overflowCount_;
			return false;
		}
		actions_[count_++] = std::move(action);
		return true;
	}

	void Clear() {
		for(int i = 0; i < count_; ++i){
			actions_[i] = {};
		}
		count_ = 0;
	}

	bool empty() const { return count_ == 0; }
	std::size_t size() const { return static_cast<std::size_t>(count_); }
	int count() const { return count_; }
	int OverflowCount() const { return overflowCount_; }
	const UiAction& front() const { return actions_[0]; }
	const UiAction& back() const { return actions_[count_ - 1]; }
	const UiAction& operator[](std::size_t index) const { return actions_[index]; }
	UiAction& operator[](std::size_t index) { return actions_[index]; }
	iterator begin() { return actions_.data(); }
	iterator end() { return actions_.data() + count_; }
	const_iterator begin() const { return actions_.data(); }
	const_iterator end() const { return actions_.data() + count_; }

private:
	std::array<UiAction, UI_ACTION_QUEUE_MAX_ACTIONS> actions_ = {};
	int count_ = 0;
	int overflowCount_ = 0;
};

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

	UiActionList Drain() {
		UiActionList out;
		for(int i = 0; i < count_; ++i){
			out.Push(std::move(actions_[i]));
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
