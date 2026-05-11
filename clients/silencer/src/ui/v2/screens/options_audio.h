#ifndef SILENCER_UI_V2_SCREENS_OPTIONS_AUDIO_H
#define SILENCER_UI_V2_SCREENS_OPTIONS_AUDIO_H

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct OptionsAudioHandlers {
	std::function<void()> on_toggle_music;
	std::function<void()> on_save;
	std::function<void()> on_cancel;
};

// Live state driving the off/on indicator sprite indices. Passed by the
// engine render path so the indicators reflect Config; preview leaves this
// nullptr to keep the build-time defaults (12, 14) and a byte-identical
// PPM diff against legacy's pre-Tick state.
struct OptionsAudioState {
	bool music = false;
};

// Returns the declarative tree for the audio-options sub-screen. Mirrors
// legacy OptionsAudioScreen::Build (clients/silencer/src/ui/screens/options/
// options_audio_screen.cpp) so rendered output is byte-identical at scale=1
// when `state` is null. When `state` is provided, indicator indices mirror
// the legacy Tick path:
//   off = state.music ? 12 : 13;   on = state.music ? 15 : 14;
Node BuildOptionsAudio(const Context & ctx, const OptionsAudioHandlers & handlers = {}, const OptionsAudioState * state = nullptr);

}  // namespace v2
}  // namespace ui

#endif
