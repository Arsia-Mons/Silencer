#ifndef SILENCER_CLIENT_UI_OPTIONS_CONTROLS_REBIND_CAPTURE_H
#define SILENCER_CLIENT_UI_OPTIONS_CONTROLS_REBIND_CAPTURE_H

// Rebind-capture support for Options→Controls: legacy two-slot view of a
// KeyMap action, label resolution for either slot, and the
// keyboard/gamepad-button/gamepad-axis "next press captures" sink.

#include "keybinds.h"
#include "runtime/UiActionQueue.h"

#include <SDL3/SDL_scancode.h>

#include <string>

class ScreenContext;

namespace silencer::client_ui::options {

// Two-key view of an action's bindings. The on-disk model is OR-of-AND with
// arbitrary list size; the UI shows a fixed primary/secondary slot pair plus
// an OR/AND toggle.
//
//   bindings = []                 → key1=UNK, key2=UNK
//   bindings = [[k1]]             → key1=k1,  key2=UNK
//   bindings = [[k1], [k2]]       → key1=k1,  key2=k2,  AND=false
//   bindings = [[k1, k2]]         → key1=k1,  key2=k2,  AND=true
struct LegacyBindingView {
	SDL_Scancode key1 = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key2 = SDL_SCANCODE_UNKNOWN;
	bool         and_ = false;
};

LegacyBindingView ViewLegacy(const KeyMap & km, Action a);
void WriteLegacy(KeyMap & km, Action a, SDL_Scancode key1, SDL_Scancode key2, bool and_);

// Display label for the slot-th binding of an action. Handles keyboard,
// gamepad button, and gamepad axis bindings.
std::string GetBindingLabel(ScreenContext & ctx, Action a, int slot);

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
