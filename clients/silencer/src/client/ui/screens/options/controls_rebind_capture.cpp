#include "controls_rebind_capture.h"

#include "screen_context.h"

#include <SDL3/SDL.h>

namespace silencer::client_ui::options {

void FinishKeyboardRebind(ScreenContext & ctx,
                          int & rebindRow, int & rebindSlot,
                          SDL_Scancode sym) {
	if(rebindRow < 0 || rebindRow >= (int)Action::Count) return;
#ifndef OUYA
	if(sym == SDL_SCANCODE_ESCAPE) sym = SDL_SCANCODE_UNKNOWN;
#endif
	Action a = ACTION_TABLE[rebindRow].action;
	ScreenContext::LegacyKeyBindingSlots v = ctx.LegacyKeyBinding(a);
	if(rebindSlot == 0) v.key1 = sym; else v.key2 = sym;
	ctx.WriteLegacyKeyBinding(a, v.key1, v.key2, v.and_);
	rebindRow = -1;
	rebindSlot = -1;
}

void FinishBindingRebind(ScreenContext & ctx,
                         int & rebindRow, int & rebindSlot,
                         const silencer::ui::UiBindingInput & input) {
	if(rebindRow < 0 || rebindRow >= (int)Action::Count) return;

	if(input.kind == silencer::ui::UiBindingInputKind::KeyboardKeyDown){
		FinishKeyboardRebind(ctx, rebindRow, rebindSlot,
		                     static_cast<SDL_Scancode>(input.code));
		return;
	}

	if(!ctx.SetCapturedBinding(ACTION_TABLE[rebindRow].action, rebindSlot, input)) return;
	rebindRow = -1;
	rebindSlot = -1;
}

}  // namespace silencer::client_ui::options
