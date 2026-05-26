#pragma once

#include "ui/focus/UiFocus.h"

namespace silencer::ui::primitives {

struct ControlState {
	bool checked = false;
	bool selected = false;
	bool disabled = false;
};

struct VisualState {
	bool targeted = false;
	bool active = false;
	bool chosen = false;
	bool unavailable = false;
};

inline VisualState DeriveVisualState(const silencer::ui::UiFocusableState& focus,
                                     const ControlState& control) {
	return {
		focus.hovered || (focus.focused && focus.focusVisible),
		focus.pressed,
		control.checked || control.selected,
		control.disabled,
	};
}

}  // namespace silencer::ui::primitives
