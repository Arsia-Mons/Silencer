#ifndef SILENCER_CLIENT_UI_OPTIONS_CONTROLS_REBIND_CAPTURE_H
#define SILENCER_CLIENT_UI_OPTIONS_CONTROLS_REBIND_CAPTURE_H

// Rebind-capture support for Options->Controls: keyboard/gamepad-button/gamepad-axis
// "next press captures" sinks. The keymap storage shape stays behind
// ScreenContext; this helper only consumes the screen's in-flight row/slot state.

#include "runtime/UiActionQueue.h"

#include <SDL3/SDL_scancode.h>

class ScreenContext;

namespace silencer::client_ui::options {

// Capture sinks. Both consume the row/slot in-flight and reset the caller's
// rebind state to (-1, -1) on success.
void FinishKeyboardRebind(ScreenContext & ctx,
                          int & rebindRow, int & rebindSlot,
                          SDL_Scancode sym);
void FinishBindingRebind(ScreenContext & ctx,
                         int & rebindRow, int & rebindSlot,
                         const silencer::ui::UiBindingInput & input);

}  // namespace silencer::client_ui::options

#endif
