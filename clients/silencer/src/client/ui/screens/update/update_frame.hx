#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace silencer {
namespace client_ui {

struct UpdateFrameProps {
	const char * key = nullptr;
	const char * status_text = nullptr;
	const char * progress_text = nullptr;
	bool can_update = false;
	bool can_cancel = false;
	bool can_retry = false;
	bool can_download = false;
	std::function<void()> start_update = {};
	std::function<void()> cancel = {};
	std::function<void()> retry = {};
	std::function<void()> download = {};
};

::ui::UiElement UpdateFrame(const UpdateFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
