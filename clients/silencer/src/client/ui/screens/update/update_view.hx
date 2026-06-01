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

struct UpdateStatus {
	const char * status = "";
	const char * progress = "";
	UpdatePrimaryAction primary_action = UpdatePrimaryAction::None;
	bool show_cancel = false;
	std::function<void()> start_update = {};
	std::function<void()> retry = {};
	std::function<bool()> download = {};
	std::function<bool()> cancel = {};
};

const UpdateStatus& UseUpdateStatus();

struct UpdateFrameProps {
	const char * key = nullptr;
};

::ui::UiElement UpdateFrame(const UpdateFrameProps& props);

struct UpdateViewProps {
	const char * key = nullptr;
	const UpdateStatus * status = nullptr;
};

::ui::UiElement UpdateView(const UpdateViewProps& props);

}  // namespace silencer::client_ui
