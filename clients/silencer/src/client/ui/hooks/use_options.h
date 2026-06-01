#pragma once

#include "client/ui/providers/options_provider.h"
#include "keybinds.h"
#include "runtime/UiActionQueue.h"

#include <SDL3/SDL_scancode.h>

#include <string>

namespace silencer {
namespace client_ui {

struct OptionsBindingView {
	SDL_Scancode key1 = SDL_SCANCODE_UNKNOWN;
	SDL_Scancode key2 = SDL_SCANCODE_UNKNOWN;
	bool and_ = false;
};

class OptionsAudioModel {
public:
	explicit OptionsAudioModel(const OptionsProviderValue& provider);

	bool music_enabled() const;
	void set_music_enabled(bool enabled) const;
	void toggle_music_enabled() const;
	void save() const;
	void cancel() const;

private:
	OptionsProviderValue provider_;
};

class OptionsDisplayModel {
public:
	explicit OptionsDisplayModel(const OptionsProviderValue& provider);

	bool fullscreen_enabled() const;
	bool smooth_scaling_enabled() const;
	void set_fullscreen_enabled(bool enabled) const;
	void set_smooth_scaling_enabled(bool enabled) const;
	void toggle_fullscreen() const;
	void toggle_smooth_scaling() const;
	void save() const;
	void cancel() const;

private:
	OptionsProviderValue provider_;
};

class OptionsControlsModel {
public:
	explicit OptionsControlsModel(const OptionsProviderValue& provider);

	std::string profile_label() const;
	OptionsBindingView binding(Action action) const;
	std::string binding_label(Action action, int slot) const;
	int tick_count() const;
	void cycle_preset() const;
	void toggle_operator(Action action) const;
	void finish_keyboard_rebind(int & rebindRow,
	                            int & rebindSlot,
	                            SDL_Scancode sym) const;
	void finish_binding_rebind(int & rebindRow,
	                           int & rebindSlot,
	                           const silencer::ui::UiBindingInput & input) const;
	void save() const;
	void cancel() const;

private:
	OptionsProviderValue provider_;
};

class OptionsModel {
public:
	explicit OptionsModel(const OptionsProviderValue& provider);

	OptionsAudioModel audio;
	OptionsDisplayModel display;
	OptionsControlsModel controls;
};

OptionsModel use_options(const OptionsProviderValue& provider);

}  // namespace client_ui
}  // namespace silencer
