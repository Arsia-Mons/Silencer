#ifndef SILENCER_CLIENT_UI_OPTIONS_CONTROLS_KEYBIND_LIST_H
#define SILENCER_CLIENT_UI_OPTIONS_CONTROLS_KEYBIND_LIST_H

// Options→Controls keybind-list UI. Owns the screen-local panel content:
// title, preset row, keybind rows, scroll-area metadata, and Save / Cancel.

#include <string>
#include <vector>

class Surface;
class OptionsControlsScreen;

namespace silencer::ui {
class UiInteractionRegistry;
}

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
};

int KeybindListVisibleRowsForContentHeight(int contentHeight);

// Emits the keybind-list panel interior into the current Clay frame. The
// caller wraps in the scalable panel chrome.
void BuildKeybindListBody(const KeybindListView & view,
                          silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::options

#endif
