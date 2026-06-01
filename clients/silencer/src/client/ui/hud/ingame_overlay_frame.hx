#pragma once

#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct InGameOverlayFrameProps {
	const char * key = nullptr;
	int width = 0;
	int height = 0;
	bool show_quit_prompt = false;
	const char * quit_prompt_text = nullptr;
};

::ui::UiElement InGameOverlayFrame(const InGameOverlayFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
