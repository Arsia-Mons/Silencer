#pragma once

#include "ui/runtime/element.h"

#include <functional>

namespace silencer {
namespace client_ui {

struct OptionsFrameProps {
	const char * key = nullptr;
	std::function<void()> open_controls = {};
	std::function<void()> open_display = {};
	std::function<void()> open_audio = {};
	std::function<void()> go_back = {};
};

::ui::UiElement OptionsFrame(const OptionsFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
