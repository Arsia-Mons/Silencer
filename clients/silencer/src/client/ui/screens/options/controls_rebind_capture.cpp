#include "controls_rebind_capture.h"

#include "screen_context.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

namespace silencer::client_ui::options {

LegacyBindingView ViewLegacy(const ScreenContext & ctx, Action a) {
	ScreenContext::LegacyKeyBindingSlots slots = ctx.LegacyKeyBinding(a);
	LegacyBindingView v;
	v.key1 = slots.key1;
	v.key2 = slots.key2;
	v.and_ = slots.and_;
	return v;
}

void WriteLegacy(ScreenContext & ctx, Action a, SDL_Scancode key1, SDL_Scancode key2, bool and_) {
	ctx.WriteLegacyKeyBinding(a, key1, key2, and_);
}

std::string GetBindingLabel(ScreenContext & ctx, Action a, int slot) {
	return ctx.KeyBindingSlotLabel(a, slot);
}

void FinishKeyboardRebind(ScreenContext & ctx,
                          int & rebindRow, int & rebindSlot,
                          SDL_Scancode sym) {
	if(rebindRow < 0 || rebindRow >= (int)Action::Count) return;
#ifndef OUYA
	if(sym == SDL_SCANCODE_ESCAPE) sym = SDL_SCANCODE_UNKNOWN;
#endif
	Action a = ACTION_TABLE[rebindRow].action;
	LegacyBindingView v = ViewLegacy(ctx, a);
	if(rebindSlot == 0) v.key1 = sym; else v.key2 = sym;
	WriteLegacy(ctx, a, v.key1, v.key2, v.and_);
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

	BindingKey padKey{};
	if(input.kind == silencer::ui::UiBindingInputKind::GamepadButtonDown){
		padKey.device = BindingDevice::GamepadButton;
		padKey.code = input.code;
		padKey.axisDir = 0;
	}else if(input.kind == silencer::ui::UiBindingInputKind::GamepadAxisMoved){
		padKey.device = BindingDevice::GamepadAxis;
		padKey.code = input.code;
		padKey.axisDir = static_cast<int8_t>(input.axisDir < 0 ? -1 : 1);
	}else{
		return;
	}

	ctx.SetCapturedBinding(ACTION_TABLE[rebindRow].action, rebindSlot, padKey);
	rebindRow = -1;
	rebindSlot = -1;
}

}  // namespace silencer::client_ui::options
