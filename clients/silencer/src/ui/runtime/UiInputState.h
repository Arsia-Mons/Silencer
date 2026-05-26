#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <utility>

#include "ui/focus/UiFocus.h"
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

constexpr int UI_INPUT_MAX_TEXT_CHARS = 256;
constexpr int UI_INPUT_MAX_NAV_ACTIONS = 64;
constexpr int UI_INPUT_MAX_BINDING_INPUTS = 64;
constexpr int UI_INPUT_MAX_CONTROL_COMMANDS = 128;

template <typename T, int Capacity>
class UiBoundedInputList {
public:
	using iterator = T *;
	using const_iterator = const T *;

	bool Push(T value) {
		if(count_ >= Capacity) {
			++overflowCount_;
			return false;
		}
		items_[count_++] = std::move(value);
		return true;
	}

	bool push_back(T value) { return Push(std::move(value)); }
	void clear() {
		for(int i = 0; i < count_; ++i){
			items_[i] = {};
		}
		count_ = 0;
	}

	bool empty() const { return count_ == 0; }
	std::size_t size() const { return static_cast<std::size_t>(count_); }
	int count() const { return count_; }
	int OverflowCount() const { return overflowCount_; }
	const T& operator[](std::size_t index) const { return items_[index]; }
	T& operator[](std::size_t index) { return items_[index]; }
	iterator begin() { return items_.data(); }
	iterator end() { return items_.data() + count_; }
	const_iterator begin() const { return items_.data(); }
	const_iterator end() const { return items_.data() + count_; }

private:
	std::array<T, Capacity> items_ = {};
	int count_ = 0;
	int overflowCount_ = 0;
};

class UiTextInputBuffer {
public:
	using iterator = char *;
	using const_iterator = const char *;

	bool push_back(char ascii) {
		if(count_ >= UI_INPUT_MAX_TEXT_CHARS) {
			++overflowCount_;
			return false;
		}
		chars_[count_++] = ascii;
		return true;
	}

	void clear() {
		for(int i = 0; i < count_; ++i){
			chars_[i] = 0;
		}
		count_ = 0;
	}

	bool empty() const { return count_ == 0; }
	std::size_t size() const { return static_cast<std::size_t>(count_); }
	int count() const { return count_; }
	int OverflowCount() const { return overflowCount_; }
	std::string ToString() const { return std::string(chars_.data(), chars_.data() + count_); }
	bool operator==(const char * text) const {
		if(!text) text = "";
		int i = 0;
		while(text[i] != '\0' && i < count_){
			if(chars_[i] != text[i]) return false;
			++i;
		}
		return i == count_ && text[i] == '\0';
	}
	bool operator==(const std::string& text) const {
		if(text.size() != size()) return false;
		for(int i = 0; i < count_; ++i){
			if(chars_[i] != text[static_cast<std::size_t>(i)]) return false;
		}
		return true;
	}
	char operator[](std::size_t index) const { return chars_[index]; }
	iterator begin() { return chars_.data(); }
	iterator end() { return chars_.data() + count_; }
	const_iterator begin() const { return chars_.data(); }
	const_iterator end() const { return chars_.data() + count_; }

private:
	std::array<char, UI_INPUT_MAX_TEXT_CHARS> chars_ = {};
	int count_ = 0;
	int overflowCount_ = 0;
};

struct UiInputState {
	// width/height are the VIRTUAL layout dimensions Clay lays out against
	// (the native surface size divided by uiScale). uiScale is the compositor
	// magnification the bitmap UI render path applies when it copies that
	// virtual layout back into the larger native surface.
	int width = 0;
	int height = 0;
	float uiScale = 1.0f;
	float deltaTimeSeconds = 0.0f;
	float animationDeltaSeconds = 1.0f / 24.0f;
	float animationStepSeconds = 1.0f / 24.0f;
	UiFocusSource source = UiFocusSource::Keyboard;
	UiPointerState pointer;
	UiTextInputBuffer textInput;
	UiBoundedInputList<UiNavAction, UI_INPUT_MAX_NAV_ACTIONS> navActions;
	UiBoundedInputList<UiBindingInput, UI_INPUT_MAX_BINDING_INPUTS> bindingInputs;
	UiBoundedInputList<UiControlCommand, UI_INPUT_MAX_CONTROL_COMMANDS> controlCommands;

	bool HasWindow() const { return width > 0 && height > 0; }
};

}  // namespace ui
}  // namespace silencer
