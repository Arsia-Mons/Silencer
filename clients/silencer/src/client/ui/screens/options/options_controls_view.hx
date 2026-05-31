#pragma once

#include "client/ui/screens/options/controls_keybind_list.h"
#include "ui/components/common.h"

#include <functional>

namespace silencer::client_ui {

constexpr int kOptionsControlsMaxRows = 30;

struct OptionsControlsViewProps {
	const char * key = nullptr;
	const options::KeybindListView * keybinds = nullptr;
	int frame_pad_left = 0;
	int frame_pad_right = 0;
	int frame_pad_top = 0;
	int frame_pad_bottom = 0;
	int panel_pad_x = 48;
	int panel_pad_bottom = 20;
	std::function<void(const ::ui::ActivationEvent&)> on_preset = {};
	std::function<void(int row, int slot)> on_rebind = {};
	std::function<void(int row)> on_operator = {};
	std::function<void(const ::ui::ActivationEvent&)> on_save = {};
	std::function<void(const ::ui::ActivationEvent&)> on_cancel = {};
};

::ui::UiElement OptionsControlsView(const OptionsControlsViewProps& props);

}  // namespace silencer::client_ui
