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
	Select,
	Cancel,
};

struct UiAction {
	UiActionKind kind = UiActionKind::None;
	std::string id;
	std::string value;
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
