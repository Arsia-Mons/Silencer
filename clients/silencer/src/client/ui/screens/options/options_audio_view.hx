#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>

namespace silencer::client_ui {

struct OptionsAudioContextValue {
	bool music = false;
	std::function<void()> toggle_music = {};
	std::function<void()> save = {};
	std::function<void()> cancel = {};
};

const OptionsAudioContextValue& UseOptionsAudio();

struct OptionsAudioViewProps {
	const char * key = nullptr;
	const OptionsAudioContextValue * value = nullptr;
};

::ui::UiElement OptionsAudioView(const OptionsAudioViewProps& props);

}  // namespace silencer::client_ui
