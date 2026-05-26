#pragma once

#include <array>
#include <cstddef>
#include <cstring>
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

constexpr int UI_ACTION_MAX_ID_CHARS = 128;
constexpr int UI_ACTION_MAX_VALUE_CHARS = 512;

template <int Capacity>
class UiBoundedText {
public:
	using const_iterator = const char *;
	static constexpr std::size_t npos = static_cast<std::size_t>(-1);

	UiBoundedText() = default;
	UiBoundedText(const char * text) { Assign(text); }

	UiBoundedText& operator=(const char * text) {
		Assign(text);
		return *this;
	}

	bool Assign(const char * text) {
		return Assign(text, text ? std::strlen(text) : 0);
	}

	bool Assign(const char * text, std::size_t len) {
		if(!text) len = 0;
		const bool fits = len <= static_cast<std::size_t>(Capacity);
		const std::size_t copyLen = fits ? len : static_cast<std::size_t>(Capacity);
		if(copyLen > 0){
			std::memcpy(chars_.data(), text, copyLen);
		}
		chars_[copyLen] = '\0';
		count_ = static_cast<int>(copyLen);
		if(!fits) ++overflowCount_;
		return fits;
	}

	void clear() {
		for(int i = 0; i < count_; ++i){
			chars_[i] = '\0';
		}
		count_ = 0;
		chars_[0] = '\0';
	}

	bool empty() const { return count_ == 0; }
	std::size_t size() const { return static_cast<std::size_t>(count_); }
	int count() const { return count_; }
	int OverflowCount() const { return overflowCount_; }
	const char * c_str() const { return chars_.data(); }
	const char * data() const { return chars_.data(); }
	const_iterator begin() const { return chars_.data(); }
	const_iterator end() const { return chars_.data() + count_; }

	int compare(std::size_t pos, std::size_t len, const char * text) const {
		if(!text) text = "";
		if(pos > size()) return 1;
		std::size_t lhsLen = size() - pos;
		if(lhsLen > len) lhsLen = len;
		const std::size_t rhsLen = std::strlen(text);
		const std::size_t common = lhsLen < rhsLen ? lhsLen : rhsLen;
		for(std::size_t i = 0; i < common; ++i){
			const unsigned char lhs = static_cast<unsigned char>(chars_[pos + i]);
			const unsigned char rhs = static_cast<unsigned char>(text[i]);
			if(lhs != rhs) return lhs < rhs ? -1 : 1;
		}
		if(lhsLen == rhsLen) return 0;
		return lhsLen < rhsLen ? -1 : 1;
	}

	std::size_t find(const char * needle) const {
		if(!needle) return npos;
		const std::size_t needleLen = std::strlen(needle);
		if(needleLen == 0) return 0;
		if(needleLen > size()) return npos;
		for(std::size_t i = 0; i + needleLen <= size(); ++i){
			if(std::memcmp(chars_.data() + i, needle, needleLen) == 0) return i;
		}
		return npos;
	}

private:
	std::array<char, Capacity + 1> chars_ = {};
	int count_ = 0;
	int overflowCount_ = 0;
};

template <int Capacity>
bool operator==(const UiBoundedText<Capacity>& lhs, const char * rhs) {
	if(!rhs) rhs = "";
	const std::size_t rhsLen = std::strlen(rhs);
	return lhs.size() == rhsLen && std::memcmp(lhs.data(), rhs, rhsLen) == 0;
}

template <int Capacity>
bool operator==(const char * lhs, const UiBoundedText<Capacity>& rhs) {
	return rhs == lhs;
}

template <int Capacity>
bool operator==(const UiBoundedText<Capacity>& lhs, const std::string& rhs) {
	return lhs.size() == rhs.size() && std::memcmp(lhs.data(), rhs.data(), rhs.size()) == 0;
}

template <int Capacity>
bool operator==(const std::string& lhs, const UiBoundedText<Capacity>& rhs) {
	return rhs == lhs;
}

template <int Capacity>
bool operator!=(const UiBoundedText<Capacity>& lhs, const char * rhs) {
	return !(lhs == rhs);
}

template <int Capacity>
bool operator!=(const char * lhs, const UiBoundedText<Capacity>& rhs) {
	return !(lhs == rhs);
}

template <int Capacity>
bool operator!=(const UiBoundedText<Capacity>& lhs, const std::string& rhs) {
	return !(lhs == rhs);
}

template <int Capacity>
bool operator!=(const std::string& lhs, const UiBoundedText<Capacity>& rhs) {
	return !(lhs == rhs);
}

using UiActionId = UiBoundedText<UI_ACTION_MAX_ID_CHARS>;
using UiActionValue = UiBoundedText<UI_ACTION_MAX_VALUE_CHARS>;

struct UiAction {
	UiActionKind kind = UiActionKind::None;
	UiActionId id;
	UiActionValue value;
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
