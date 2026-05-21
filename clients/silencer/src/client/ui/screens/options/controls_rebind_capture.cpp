#include "controls_rebind_capture.h"

#include "screen_context.h"
#include "game.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <utility>

namespace silencer::client_ui::options {

LegacyBindingView ViewLegacy(const KeyMap & km, Action a) {
	LegacyBindingView v;
	const auto & ab = km.Get(a);
	if(ab.bindings.empty()) return v;
	const auto & b0 = ab.bindings[0];
	if(b0.keys.size() >= 2 &&
	   b0.keys[0].device == BindingDevice::Keyboard &&
	   b0.keys[1].device == BindingDevice::Keyboard){
		v.key1 = (SDL_Scancode)b0.keys[0].code;
		v.key2 = (SDL_Scancode)b0.keys[1].code;
		v.and_ = true;
		return v;
	}
	if(!b0.keys.empty() && b0.keys[0].device == BindingDevice::Keyboard){
		v.key1 = (SDL_Scancode)b0.keys[0].code;
	}
	if(ab.bindings.size() >= 2){
		const auto & b1 = ab.bindings[1];
		if(!b1.keys.empty() && b1.keys[0].device == BindingDevice::Keyboard){
			v.key2 = (SDL_Scancode)b1.keys[0].code;
		}
	}
	return v;
}

void WriteLegacy(KeyMap & km, Action a, SDL_Scancode key1, SDL_Scancode key2, bool and_) {
	auto & ab = km.Get(a);
	ab.bindings.clear();
	auto mk = [](SDL_Scancode sc){
		BindingKey k;
		k.device  = BindingDevice::Keyboard;
		k.code    = (int)sc;
		k.axisDir = 0;
		return k;
	};
	if(key1 == SDL_SCANCODE_UNKNOWN && key2 == SDL_SCANCODE_UNKNOWN) return;
	if(and_ && key1 != SDL_SCANCODE_UNKNOWN && key2 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key1)); b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
		return;
	}
	if(key1 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key1));
		ab.bindings.push_back(std::move(b));
	}
	if(key2 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
	}
}

std::string GetBindingLabel(ScreenContext & ctx, Action a, int slot) {
	const auto & ab = ctx.keymap.Get(a);
	int found = 0;
	for(const auto & b : ab.bindings){
		if(b.keys.empty()) continue;
		if(found == slot){
			const auto & k = b.keys[0];
			if(k.device == BindingDevice::Keyboard){
				return KeyMap::GetKeyName((SDL_Scancode)k.code);
			}
			std::string s = Stringify(k);
			auto colon = s.find(':');
			std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
			SDL_Gamepad * pad = ctx.game.GetGamepad();
			return GamepadShortLabel(raw, pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN);
		}
		found++;
	}
	return KeyMap::GetKeyName(SDL_SCANCODE_UNKNOWN);
}

void FinishKeyboardRebind(ScreenContext & ctx,
                          int & rebindRow, int & rebindSlot,
                          SDL_Scancode sym) {
	if(rebindRow < 0 || rebindRow >= (int)Action::Count) return;
#ifndef OUYA
	if(sym == SDL_SCANCODE_ESCAPE) sym = SDL_SCANCODE_UNKNOWN;
#endif
	Action a = ACTION_TABLE[rebindRow].action;
	LegacyBindingView v = ViewLegacy(ctx.keymap, a);
	if(rebindSlot == 0) v.key1 = sym; else v.key2 = sym;
	ForkActiveProfileIfBuiltin(ctx.keymap);
	WriteLegacy(ctx.keymap, a, v.key1, v.key2, v.and_);
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

	ForkActiveProfileIfBuiltin(ctx.keymap);
	auto & ab = ctx.keymap.Get(ACTION_TABLE[rebindRow].action);
	Binding binding;
	binding.keys.push_back(padKey);
	if(rebindSlot == 0){
		if(ab.bindings.empty()) ab.bindings.push_back(binding);
		else ab.bindings[0] = binding;
	}else{
		if(ab.bindings.empty()) ab.bindings.push_back(Binding{});
		if(ab.bindings.size() < 2) ab.bindings.push_back(binding);
		else ab.bindings[1] = binding;
	}
	rebindRow = -1;
	rebindSlot = -1;
}

}  // namespace silencer::client_ui::options
