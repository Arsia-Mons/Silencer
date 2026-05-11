#ifndef SILENCER_UI_V2_SCREENS_OPTIONS_AUDIO_H
#define SILENCER_UI_V2_SCREENS_OPTIONS_AUDIO_H

#include "runtime.h"
#include "ui_state.h"

#include <functional>

class World;
class ScreenContext;

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

// Engine-side runtime for GameState::OPTIONSAUDIO.
class OptionsAudioRuntime : public Runtime
{
public:
	OptionsAudioRuntime(World & world, ScreenContext & sctx);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt,
	            int logical_w, int logical_h, int scale) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y,
	                       int logical_w, int logical_h, int scale) override;

private:
	World &         world_;
	ScreenContext & sctx_;
	UIState         state_;
};

}  // namespace v2
}  // namespace ui

#endif
