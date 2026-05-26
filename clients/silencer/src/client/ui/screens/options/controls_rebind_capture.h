#ifndef SILENCER_CLIENT_UI_OPTIONS_CONTROLS_REBIND_CAPTURE_H
#define SILENCER_CLIENT_UI_OPTIONS_CONTROLS_REBIND_CAPTURE_H

// Rebind-capture support for Options->Controls: keyboard/gamepad-button/gamepad-axis
// "next press captures" sinks. The keymap storage shape stays behind
// ScreenContext; this helper only consumes the screen's in-flight row/slot state.

#include "runtime/UiActionQueue.h"

#include <SDL3/SDL_scancode.h>

class ScreenContext;

namespace silencer::client_ui::options {

// Capture sinks. The caller owns in-flight row/slot state and decides when to
// clear it; these helpers only apply a validated row/slot to ScreenContext.
bool ApplyKeyboardRebind(ScreenContext & ctx,
                         int row, int slot,
                         SDL_Scancode sym);
bool ApplyBindingRebind(ScreenContext & ctx,
                        int row, int slot,
                        const silencer::ui::UiBindingInput & input);

}  // namespace silencer::client_ui::options

#endif
