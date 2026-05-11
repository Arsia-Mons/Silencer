#ifndef SILENCER_UI_V2_SCREENS_OPTIONS_DISPLAY_H
#define SILENCER_UI_V2_SCREENS_OPTIONS_DISPLAY_H

#include "runtime.h"

#include <functional>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Context;

struct OptionsDisplayHandlers {
	std::function<void()> on_toggle_fullscreen;
	std::function<void()> on_toggle_smooth_scaling;
	std::function<void()> on_save;
	std::function<void()> on_cancel;
};

struct OptionsDisplayState {
	bool fullscreen = false;
	bool scalefilter = false;
};

void RenderOptionsDisplay(const Context & ctx, const OptionsDisplayHandlers & handlers, const OptionsDisplayState & state);

// Engine-side runtime for GameState::OPTIONSDISPLAY. Constructed by
// Game::SetRuntime on state entry; reads live Config each frame to
// drive the off/on indicator sprites.
class OptionsDisplayRuntime : public Runtime
{
public:
	OptionsDisplayRuntime(World & world, ScreenContext & sctx);

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
