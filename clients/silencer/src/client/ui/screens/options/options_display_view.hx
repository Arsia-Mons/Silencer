#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>

namespace silencer::client_ui {

struct OptionsDisplayContextValue {
	bool fullscreen = false;
	bool smooth_scaling = false;
	std::function<void()> toggle_fullscreen = {};
	std::function<void()> toggle_smooth_scaling = {};
	std::function<void()> save = {};
	std::function<void()> cancel = {};
};

const OptionsDisplayContextValue& UseOptionsDisplay();

struct OptionsDisplayFrameProps {
	const char * key = nullptr;
};

::ui::UiElement OptionsDisplayFrame(const OptionsDisplayFrameProps& props);

struct OptionsDisplayViewProps {
	const char * key = nullptr;
	const OptionsDisplayContextValue * value = nullptr;
};

::ui::UiElement OptionsDisplayView(const OptionsDisplayViewProps& props);

}  // namespace silencer::client_ui
