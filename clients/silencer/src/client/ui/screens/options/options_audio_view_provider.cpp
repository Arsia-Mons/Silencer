#include "client/ui/screens/options/options_audio_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext OptionsAudioContext = {};
const OptionsAudioContextValue kEmptyOptionsAudio = {};
}  // namespace

const OptionsAudioContextValue& UseOptionsAudio() {
	const auto * value = static_cast<const OptionsAudioContextValue *>(
		::use_context(&OptionsAudioContext));
	if(value) return *value;
	::react_report_error("client/ui/options: missing OptionsAudioProvider for UseOptionsAudio\n");
	return kEmptyOptionsAudio;
}

::ui::UiElement OptionsAudioView(const OptionsAudioViewProps& props) {
	const OptionsAudioContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyOptionsAudio);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"OptionsAudioProvider",
		&OptionsAudioContext,
		const_cast<OptionsAudioContextValue *>(stored),
		::ui::children({
			::ui::component("OptionsAudioFrame",
			                OptionsAudioFrameProps{ .key = "frame" },
			                OptionsAudioFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
