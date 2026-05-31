#pragma once

#include "ui/components/common.h"

#include <functional>

namespace silencer::client_ui {

struct OptionsAudioViewProps {
	const char * key = nullptr;
	bool music = false;
	std::function<void(bool)> on_music = {};
	std::function<void(const ::ui::ActivationEvent&)> on_save = {};
	std::function<void(const ::ui::ActivationEvent&)> on_cancel = {};
};

::ui::UiElement OptionsAudioView(const OptionsAudioViewProps& props);

}  // namespace silencer::client_ui
