#include "client/ui/screens/options/options_audio_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext OptionsAudioContext = {};
const OptionsAudio kEmptyOptionsAudio = {};
}  // namespace

const OptionsAudio& UseOptionsAudio() {
	const auto * value = static_cast<const OptionsAudio *>(
		::use_context(&OptionsAudioContext));
	if(value) return *value;
	::react_report_error("client/ui/options: missing OptionsAudioProvider for UseOptionsAudio\n");
	return kEmptyOptionsAudio;
}

::ui::UiElement OptionsAudioView(const OptionsAudioViewProps& props) {
	const OptionsAudio * stored = ::ui::copy_value(
		props.audio ? *props.audio : kEmptyOptionsAudio);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"OptionsAudioProvider",
		&OptionsAudioContext,
		const_cast<OptionsAudio *>(stored),
		::ui::children({
			::ui::component("OptionsAudioFrame",
			                OptionsAudioFrameProps{ .key = "frame" },
			                OptionsAudioFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
