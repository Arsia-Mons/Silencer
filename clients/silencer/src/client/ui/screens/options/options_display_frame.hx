#pragma once

#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct OptionsDisplayFrameProps {
	const char * key = nullptr;
	bool fullscreen_enabled = false;
	bool smooth_scaling_enabled = false;
};

::ui::UiElement OptionsDisplayFrame(const OptionsDisplayFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
