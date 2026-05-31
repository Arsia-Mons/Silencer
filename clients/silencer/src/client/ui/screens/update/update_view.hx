#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>

namespace silencer::client_ui {

enum class UpdatePrimaryAction {
	None,
	Update,
	Retry,
	Download,
};

struct UpdateState {
	const char * status = "";
	const char * progress = "";
	UpdatePrimaryAction primary_action = UpdatePrimaryAction::None;
	bool show_cancel = false;
};

struct UpdateActions {
	std::function<void()> start_update = {};
	std::function<void()> retry = {};
	std::function<bool()> download = {};
	std::function<bool()> cancel = {};
};

struct UpdateContextValue {
	UpdateState state = {};
	UpdateActions actions = {};
};

extern ::ReactContext UpdateContext;

const UpdateContextValue& UseUpdate();

struct UpdateViewProps {
	const char * key = nullptr;
	const UpdateContextValue * value = nullptr;
};

::ui::UiElement UpdateView(const UpdateViewProps& props);

}  // namespace silencer::client_ui
