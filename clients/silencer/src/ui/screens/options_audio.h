#ifndef SILENCER_UI_V2_SCREENS_OPTIONS_AUDIO_H
#define SILENCER_UI_V2_SCREENS_OPTIONS_AUDIO_H

#include "runtime.h"

#include <functional>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Context;

struct OptionsAudioHandlers {
	std::function<void()> on_toggle_music;
	std::function<void()> on_save;
	std::function<void()> on_cancel;
};

// Emits the audio-options CLAY() tree directly. `music_on` flips the
// toggle button's label. Caller drives Clay_BeginLayout / EndLayout;
// `handlers` must outlive Clay_EndLayout — Clay_OnHover captures
// pointers into it as callback userData.
void RenderOptionsAudio(const Context & ctx, const OptionsAudioHandlers & handlers,
                        bool music_on);

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
};

}  // namespace v2
}  // namespace ui

#endif
