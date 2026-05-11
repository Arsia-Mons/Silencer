#ifndef SILENCER_UI_V2_SCREENS_MAIN_MENU_H
#define SILENCER_UI_V2_SCREENS_MAIN_MENU_H

#include "runtime.h"

#include <functional>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Context;

// One handler per main-menu button. Any field left empty means "no action"
// — the button still renders + hovers, the click just does nothing.
struct MainMenuHandlers {
	std::function<void()> on_tutorial;
	std::function<void()> on_lobby;
	std::function<void()> on_options;
	std::function<void()> on_exit;
};

// Emits the main-menu CLAY() tree directly into the active Clay layout
// scope. Caller is responsible for Clay_BeginLayout / Clay_EndLayout +
// pointer / scroll state plumbing. `handlers` must outlive the
// surrounding Clay_EndLayout call — Clay_OnHover captures pointers
// into it as callback userData.
void RenderMainMenu(const Context & ctx, const MainMenuHandlers & handlers);

// Engine-side runtime: owns the main-menu state and drives the Clay
// lifecycle each frame (SetPointerState → Begin/EndLayout →
// DrawRenderCommands). Constructed by Game::SetRuntime when the engine
// enters GameState::MAINMENU; destroyed on exit.
class MainMenuRuntime : public Runtime
{
public:
	MainMenuRuntime(World & world, ScreenContext & sctx);

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
