#include "client/ui/hooks/use_options.h"

#include "audio.h"
#include "config.h"
#include "game.h"
#include "renderdevice.h"
#include "screen_context.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_video.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace silencer {
namespace client_ui {

struct OptionsProviderState {
	KeyMap * keymap = nullptr;
	SDL_Window * window = nullptr;
	RenderDevice * renderdevice = nullptr;
	SDL_Gamepad * gamepad = nullptr;
	const Uint32 * tick_count = nullptr;
};

OptionsProviderValue MakeOptionsProvider(ScreenContext& ctx) {
	OptionsProviderValue value;
	value.state = std::make_shared<OptionsProviderState>();
	value.state->keymap = &ctx.keymap;
	value.state->window = ctx.window;
	value.state->renderdevice = ctx.renderdevice;
	value.state->gamepad = ctx.game.GetGamepad();
	value.state->tick_count = &ctx.world.tickcount;
	return value;
}

namespace options_provider_detail {

Config& ConfigRef() {
	return Config::GetInstance();
}

const OptionsProviderState * State(const OptionsProviderValue& provider) {
	return provider.state.get();
}

KeyMap * Keymap(const OptionsProviderValue& provider) {
	const OptionsProviderState * state = State(provider);
	return state ? state->keymap : nullptr;
}

SDL_Window * Window(const OptionsProviderValue& provider) {
	const OptionsProviderState * state = State(provider);
	return state ? state->window : nullptr;
}

RenderDevice * Device(const OptionsProviderValue& provider) {
	const OptionsProviderState * state = State(provider);
	return state ? state->renderdevice : nullptr;
}

SDL_Gamepad * Gamepad(const OptionsProviderValue& provider) {
	const OptionsProviderState * state = State(provider);
	return state ? state->gamepad : nullptr;
}

bool IsBuiltinKeybindProfile(const std::string & name) {
	return name == "default" || name == "wasd" || name == "gamepad";
}

void ApplyMusicSetting(bool on) {
	if(on){
		Audio::GetInstance().ResumeMusic();
	}else{
		Audio::GetInstance().PauseMusic();
	}
}

void ApplyDisplaySetting(const OptionsProviderValue& provider) {
	Config & cfg = ConfigRef();
	if(RenderDevice * renderdevice = Device(provider)){
		renderdevice->SetScaleFilter(cfg.scalefilter);
	}
	if(SDL_Window * window = Window(provider)){
		SDL_SetWindowFullscreen(window, cfg.fullscreen);
	}
}

OptionsBindingView ViewBinding(const KeyMap & km, Action a) {
	OptionsBindingView v;
	const auto & ab = km.Get(a);
	if(ab.bindings.empty()) return v;
	const auto & b0 = ab.bindings[0];
	if(b0.keys.size() >= 2 &&
	   b0.keys[0].device == BindingDevice::Keyboard &&
	   b0.keys[1].device == BindingDevice::Keyboard){
		v.key1 = static_cast<SDL_Scancode>(b0.keys[0].code);
		v.key2 = static_cast<SDL_Scancode>(b0.keys[1].code);
		v.and_ = true;
		return v;
	}
	if(!b0.keys.empty() && b0.keys[0].device == BindingDevice::Keyboard){
		v.key1 = static_cast<SDL_Scancode>(b0.keys[0].code);
	}
	if(ab.bindings.size() >= 2){
		const auto & b1 = ab.bindings[1];
		if(!b1.keys.empty() && b1.keys[0].device == BindingDevice::Keyboard){
			v.key2 = static_cast<SDL_Scancode>(b1.keys[0].code);
		}
	}
	return v;
}

void WriteBinding(KeyMap & km,
                  Action a,
                  SDL_Scancode key1,
                  SDL_Scancode key2,
                  bool and_) {
	auto & ab = km.Get(a);
	ab.bindings.clear();
	auto mk = [](SDL_Scancode sc){
		BindingKey k;
		k.device = BindingDevice::Keyboard;
		k.code = static_cast<int>(sc);
		k.axisDir = 0;
		return k;
	};
	if(key1 == SDL_SCANCODE_UNKNOWN && key2 == SDL_SCANCODE_UNKNOWN) return;
	if(and_ && key1 != SDL_SCANCODE_UNKNOWN && key2 != SDL_SCANCODE_UNKNOWN){
		Binding b;
		b.keys.push_back(mk(key1));
		b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
		return;
	}
	if(key1 != SDL_SCANCODE_UNKNOWN){
		Binding b;
		b.keys.push_back(mk(key1));
		ab.bindings.push_back(std::move(b));
	}
	if(key2 != SDL_SCANCODE_UNKNOWN){
		Binding b;
		b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
	}
}

std::string BindingLabel(const OptionsProviderValue& provider, Action a, int slot) {
	KeyMap * keymap = Keymap(provider);
	if(!keymap) return KeyMap::GetKeyName(SDL_SCANCODE_UNKNOWN);
	const auto & ab = keymap->Get(a);
	int found = 0;
	for(const auto & b : ab.bindings){
		if(b.keys.empty()) continue;
		if(found == slot){
			const auto & k = b.keys[0];
			if(k.device == BindingDevice::Keyboard){
				return KeyMap::GetKeyName(static_cast<SDL_Scancode>(k.code));
			}
			std::string s = Stringify(k);
			auto colon = s.find(':');
			std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
			SDL_Gamepad * pad = Gamepad(provider);
			return GamepadShortLabel(raw, pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN);
		}
		found++;
	}
	return KeyMap::GetKeyName(SDL_SCANCODE_UNKNOWN);
}

}  // namespace options_provider_detail

OptionsAudioModel::OptionsAudioModel(const OptionsProviderValue& provider)
	: provider_(provider) {}

bool OptionsAudioModel::music_enabled() const {
	return options_provider_detail::ConfigRef().music;
}

void OptionsAudioModel::set_music_enabled(bool enabled) const {
	Config & cfg = options_provider_detail::ConfigRef();
	cfg.music = enabled;
	options_provider_detail::ApplyMusicSetting(cfg.music);
}

void OptionsAudioModel::toggle_music_enabled() const {
	set_music_enabled(!music_enabled());
}

void OptionsAudioModel::save() const {
	options_provider_detail::ConfigRef().Save();
}

void OptionsAudioModel::cancel() const {
	Config & cfg = options_provider_detail::ConfigRef();
	cfg.Load();
	options_provider_detail::ApplyMusicSetting(cfg.music);
}

OptionsDisplayModel::OptionsDisplayModel(const OptionsProviderValue& provider)
	: provider_(provider) {}

bool OptionsDisplayModel::fullscreen_enabled() const {
	return options_provider_detail::ConfigRef().fullscreen;
}

bool OptionsDisplayModel::smooth_scaling_enabled() const {
	return options_provider_detail::ConfigRef().scalefilter;
}

void OptionsDisplayModel::set_fullscreen_enabled(bool enabled) const {
	options_provider_detail::ConfigRef().fullscreen = enabled;
	if(SDL_Window * window = options_provider_detail::Window(provider_)){
		SDL_SetWindowFullscreen(window, enabled);
	}
}

void OptionsDisplayModel::set_smooth_scaling_enabled(bool enabled) const {
	options_provider_detail::ConfigRef().scalefilter = enabled;
	if(RenderDevice * renderdevice = options_provider_detail::Device(provider_)){
		renderdevice->SetScaleFilter(enabled);
	}
}

void OptionsDisplayModel::toggle_fullscreen() const {
	set_fullscreen_enabled(!fullscreen_enabled());
}

void OptionsDisplayModel::toggle_smooth_scaling() const {
	set_smooth_scaling_enabled(!smooth_scaling_enabled());
}

void OptionsDisplayModel::save() const {
	options_provider_detail::ConfigRef().Save();
}

void OptionsDisplayModel::cancel() const {
	options_provider_detail::ConfigRef().Load();
	options_provider_detail::ApplyDisplaySetting(provider_);
}

OptionsControlsModel::OptionsControlsModel(const OptionsProviderValue& provider)
	: provider_(provider) {}

std::string OptionsControlsModel::profile_label() const {
	KeyMap * keymap = options_provider_detail::Keymap(provider_);
	if(keymap && !keymap->label.empty()) return keymap->label;
	if(keymap && !keymap->name.empty()) return keymap->name;
	return std::string(options_provider_detail::ConfigRef().active_keybind_profile);
}

OptionsBindingView OptionsControlsModel::binding(Action action) const {
	KeyMap * keymap = options_provider_detail::Keymap(provider_);
	if(!keymap) return OptionsBindingView{};
	return options_provider_detail::ViewBinding(*keymap, action);
}

std::string OptionsControlsModel::binding_label(Action action, int slot) const {
	return options_provider_detail::BindingLabel(provider_, action, slot);
}

int OptionsControlsModel::tick_count() const {
	const OptionsProviderState * state = options_provider_detail::State(provider_);
	return state && state->tick_count ? static_cast<int>(*state->tick_count) : 0;
}

void OptionsControlsModel::cycle_preset() const {
	if(KeyMap * keymap = options_provider_detail::Keymap(provider_)){
		CycleKeybindPreset(*keymap);
	}
}

void OptionsControlsModel::toggle_operator(Action action) const {
	KeyMap * keymap = options_provider_detail::Keymap(provider_);
	if(!keymap) return;
	OptionsBindingView v = options_provider_detail::ViewBinding(*keymap, action);
	v.and_ = !v.and_;
	ForkActiveProfileIfBuiltin(*keymap);
	options_provider_detail::WriteBinding(*keymap, action, v.key1, v.key2, v.and_);
}

void OptionsControlsModel::finish_keyboard_rebind(int & rebindRow,
                                                  int & rebindSlot,
                                                  SDL_Scancode sym) const {
	KeyMap * keymap = options_provider_detail::Keymap(provider_);
	if(!keymap || rebindRow < 0 || rebindRow >= static_cast<int>(Action::Count)) return;
#ifndef OUYA
	if(sym == SDL_SCANCODE_ESCAPE) sym = SDL_SCANCODE_UNKNOWN;
#endif
	Action action = ACTION_TABLE[rebindRow].action;
	OptionsBindingView v = options_provider_detail::ViewBinding(*keymap, action);
	if(rebindSlot == 0) v.key1 = sym; else v.key2 = sym;
	ForkActiveProfileIfBuiltin(*keymap);
	options_provider_detail::WriteBinding(*keymap, action, v.key1, v.key2, v.and_);
	rebindRow = -1;
	rebindSlot = -1;
}

void OptionsControlsModel::finish_binding_rebind(
		int & rebindRow,
		int & rebindSlot,
		const silencer::ui::UiBindingInput & input) const {
	KeyMap * keymap = options_provider_detail::Keymap(provider_);
	if(!keymap || rebindRow < 0 || rebindRow >= static_cast<int>(Action::Count)) return;

	if(input.kind == silencer::ui::UiBindingInputKind::KeyboardKeyDown){
		finish_keyboard_rebind(rebindRow,
		                       rebindSlot,
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

	ForkActiveProfileIfBuiltin(*keymap);
	auto & ab = keymap->Get(ACTION_TABLE[rebindRow].action);
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

void OptionsControlsModel::save() const {
	KeyMap * keymap = options_provider_detail::Keymap(provider_);
	Config & cfg = options_provider_detail::ConfigRef();
	const std::string active = cfg.active_keybind_profile;
	if(keymap && !options_provider_detail::IsBuiltinKeybindProfile(active)){
		keymap->SaveFile(WritableProfilePath(active));
	}
	cfg.Save();
}

void OptionsControlsModel::cancel() const {
	if(KeyMap * keymap = options_provider_detail::Keymap(provider_)){
		LoadActiveKeymap(*keymap);
	}
	options_provider_detail::ConfigRef().Load();
}

OptionsModel::OptionsModel(const OptionsProviderValue& provider)
	: audio(provider), display(provider), controls(provider) {}

OptionsModel use_options(const OptionsProviderValue& provider) {
	return OptionsModel(provider);
}

}  // namespace client_ui
}  // namespace silencer
