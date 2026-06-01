#pragma once

#include "client/ui/screens/options/controls_keybind_list.h"
#include "ui/runtime/element.h"

namespace silencer {
namespace client_ui {

struct OptionsControlsFrameProps {
	const char * key = nullptr;
	const options::KeybindListView * view = nullptr;
	int frame_pad_left = 0;
	int frame_pad_right = 0;
	int frame_pad_top = 0;
	int frame_pad_bottom = 0;
	int panel_pad_x = 48;
	int panel_pad_bottom = 0;
};

::ui::UiElement OptionsControlsFrame(const OptionsControlsFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
