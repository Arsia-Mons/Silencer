#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace silencer {
namespace client_ui {

struct OptionsDisplayFrameProps {
	const char * key = nullptr;
	bool fullscreen_enabled = false;
	bool smooth_scaling_enabled = false;
	std::function<void()> toggle_fullscreen = {};
	std::function<void()> toggle_smooth_scaling = {};
	std::function<void()> save = {};
	std::function<void()> cancel = {};
};

::ui::UiElement OptionsDisplayFrame(const OptionsDisplayFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
