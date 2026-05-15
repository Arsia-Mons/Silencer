#ifndef SILENCER_CLIENT_UI_OPTIONS_CONTROLS_KEYBIND_LIST_H
#define SILENCER_CLIENT_UI_OPTIONS_CONTROLS_KEYBIND_LIST_H

// Options→Controls keybind-list UI. Owns:
//   * The action / preset / save / cancel / scroll button primitives.
//   * The visible keybind row composition (action label + primary button +
//     OR/AND operator button + secondary button).
//   * Interaction registration for the visible row buttons.
//
// The panel chrome (root background, "Configure Controls" title) lives in the
// screen file; this header emits the panel-interior rows + the bottom action
// row.

#include <string>

class Surface;
class OptionsControlsScreen;

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::client_ui::options {

constexpr int kKeybindListVisibleRows = 5;

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
	KeybindRowView rows[kKeybindListVisibleRows];
	int visibleRowCount = 0;
};

// Registers interaction hit-rects for the preset / row buttons / scroll /
// save / cancel buttons at their legacy screen positions.
void RegisterKeybindListWidgets(int surfaceW,
                                silencer::ui::UiInteractionRegistry& interactions);

// Emits the keybind-list panel interior (preset row + visible rows + scroll
// row + save/cancel row) into the current Clay frame. The caller wraps in the
// panel background.
void BuildKeybindListBody(const KeybindListView & view,
                          silencer::ui::UiInteractionRegistry& interactions);

}  // namespace silencer::client_ui::options

#endif
