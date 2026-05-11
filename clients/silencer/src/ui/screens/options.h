#ifndef SILENCER_UI_V2_SCREENS_OPTIONS_H
#define SILENCER_UI_V2_SCREENS_OPTIONS_H

#include "runtime.h"

#include <functional>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Context;

// One handler per options-router button. Any field left empty means "no
// action" — the button still renders + hovers, the click just does nothing.
struct OptionsHandlers {
	std::function<void()> on_controls;
	std::function<void()> on_display;
	std::function<void()> on_audio;
	std::function<void()> on_go_back;
};

// Emits the options-router CLAY() tree directly into the active Clay
// layout scope. Caller is responsible for Clay_BeginLayout /
// Clay_EndLayout + pointer / scroll state plumbing. `handlers` must
// outlive the surrounding Clay_EndLayout call — Clay_OnHover captures
// pointers into it as callback userData.
void RenderOptions(const Context & ctx, const OptionsHandlers & handlers);

// Engine-side runtime: drives the Clay lifecycle each frame
// (SetPointerState → Begin/EndLayout → DrawRenderCommands).
class OptionsRuntime : public Runtime
{
public:
	OptionsRuntime(World & world, ScreenContext & sctx);

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
