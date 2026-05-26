#include "controls_rebind_capture.h"

#include "action_catalog.h"
#include "screen_context.h"

#include <SDL3/SDL.h>

namespace silencer::client_ui::options {

bool ApplyKeyboardRebind(ScreenContext & ctx,
                         int row, int slot,
                         SDL_Scancode sym) {
	if(row < 0 || row >= (int)Action::Count) return false;
#ifndef OUYA
	if(sym == SDL_SCANCODE_ESCAPE) sym = SDL_SCANCODE_UNKNOWN;
#endif
	Action a = ACTION_TABLE[row].action;
	ScreenContext::LegacyKeyBindingSlots v = ctx.LegacyKeyBinding(a);
	if(slot == 0) v.key1 = sym; else v.key2 = sym;
	ctx.WriteLegacyKeyBinding(a, v.key1, v.key2, v.and_);
	return true;
}

bool ApplyBindingRebind(ScreenContext & ctx,
                        int row, int slot,
                        const silencer::ui::UiBindingInput & input) {
	if(row < 0 || row >= (int)Action::Count) return false;

	if(input.kind == silencer::ui::UiBindingInputKind::KeyboardKeyDown){
		return ApplyKeyboardRebind(ctx, row, slot,
		                           static_cast<SDL_Scancode>(input.code));
	}

	if(!ctx.SetCapturedBinding(ACTION_TABLE[row].action, slot, input)) return false;
	return true;
}

}  // namespace silencer::client_ui::options
