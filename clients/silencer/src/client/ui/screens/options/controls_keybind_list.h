#ifndef SILENCER_CLIENT_UI_OPTIONS_CONTROLS_KEYBIND_LIST_H
#define SILENCER_CLIENT_UI_OPTIONS_CONTROLS_KEYBIND_LIST_H

// Options Controls keybind-list data shared by the screen and cppx view.

#include <string>
#include <vector>

namespace silencer::client_ui::options {

constexpr int kKeybindListMinVisibleRows = 5;
constexpr const char * kKeybindListScrollId = "options_controls.list";
constexpr const char * kKeybindListScrollLabel = "Controls List";

struct KeybindRowView {
	std::string actionLabel;
	std::string primaryLabel;
	std::string secondaryLabel;
	std::string operatorLabel;
	bool rebindingPrimary = false;
	bool rebindingSecondary = false;
};

struct KeybindListView {
	std::string presetText;
	std::vector<KeybindRowView> rows;
	int visibleRowCount = 0;
	float titleOffsetY = 8.0f;
	// Horizontal scale (<= 1) applied to the list's hardcoded legacy-pixel
	// widths so the panel interior shrinks with the window instead of
	// overflowing it at small sizes (issue #179 follow-up). 1.0 == legacy
	// design width (640-wide viewport); set by the screen.
	float hScale = 1.0f;
};

int KeybindListVisibleRowsForContentHeight(int contentHeight);

}  // namespace silencer::client_ui::options

#endif
