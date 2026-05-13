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
	void (*onClick)(void * user) = nullptr;
	void * clickUser = nullptr;
	void (*onClickRow)(void * user, int index) = nullptr;
	int rowIndex = -1;
	void (*onEnter)(void * user) = nullptr;
	void * enterUser = nullptr;
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

inline void DispatchUiActions(const std::vector<UiAction>& actions) {
	for(const UiAction& action : actions){
		switch(action.kind){
			case UiActionKind::Activate:
				if(action.onClick) action.onClick(action.clickUser);
				else if(action.onEnter) action.onEnter(action.enterUser);
				break;
			case UiActionKind::Select:
				if(action.onClickRow) action.onClickRow(action.clickUser, action.rowIndex);
				break;
			case UiActionKind::None:
			case UiActionKind::Navigate:
			case UiActionKind::SetText:
			case UiActionKind::Cancel:
				break;
		}
	}
}

}  // namespace ui
}  // namespace silencer
