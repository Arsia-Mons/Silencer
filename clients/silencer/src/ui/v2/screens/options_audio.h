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

Node BuildOptionsAudio(const Context & ctx, const OptionsAudioHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
