#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <functional>

namespace silencer::client_ui {

struct OptionsAudio {
	bool music = false;
	std::function<void()> toggle_music = {};
	std::function<void()> save = {};
	std::function<void()> cancel = {};
};

const OptionsAudio& UseOptionsAudio();

struct OptionsAudioFrameProps {
	const char * key = nullptr;
};

::ui::UiElement OptionsAudioFrame(const OptionsAudioFrameProps& props);

struct OptionsAudioViewProps {
	const char * key = nullptr;
	const OptionsAudio * audio = nullptr;
};

::ui::UiElement OptionsAudioView(const OptionsAudioViewProps& props);

}  // namespace silencer::client_ui
