#pragma once

#include "client/ui/screens/options/controls_keybind_list.h"
#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>

namespace silencer::client_ui {

constexpr int kOptionsControlsMaxRows = 30;

struct OptionsControlsContextValue {
	const options::KeybindListView * keybinds = nullptr;
	int frame_pad_left = 0;
	int frame_pad_right = 0;
	int frame_pad_top = 0;
	int frame_pad_bottom = 0;
	int panel_pad_x = 48;
	int panel_pad_bottom = 20;
	std::function<void()> cycle_preset = {};
	std::function<void(int row, int slot)> begin_rebind = {};
	std::function<void(int row)> toggle_operator = {};
	std::function<void()> save = {};
	std::function<void()> cancel = {};
};

const OptionsControlsContextValue& UseOptionsControls();

struct OptionsControlsFrameProps {
	const char * key = nullptr;
};

::ui::UiElement OptionsControlsFrame(const OptionsControlsFrameProps& props);

struct OptionsControlsViewProps {
	const char * key = nullptr;
	const OptionsControlsContextValue * value = nullptr;
};

::ui::UiElement OptionsControlsView(const OptionsControlsViewProps& props);

}  // namespace silencer::client_ui
