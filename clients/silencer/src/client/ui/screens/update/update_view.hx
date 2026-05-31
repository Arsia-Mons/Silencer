#pragma once

#include "ui/components/common.h"

#include <functional>

namespace silencer::client_ui {

enum class UpdatePrimaryAction {
	None,
	Update,
	Retry,
	Download,
};

struct UpdateViewProps {
	const char * key = nullptr;
	const char * status = "";
	const char * progress = "";
	UpdatePrimaryAction primary_action = UpdatePrimaryAction::None;
	bool show_cancel = false;
	std::function<void(const ::ui::ActivationEvent&)> on_update = {};
	std::function<void(const ::ui::ActivationEvent&)> on_retry = {};
	std::function<void(const ::ui::ActivationEvent&)> on_download = {};
	std::function<void(const ::ui::ActivationEvent&)> on_cancel = {};
};

::ui::UiElement UpdateView(const UpdateViewProps& props);

}  // namespace silencer::client_ui
